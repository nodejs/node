// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/random-rescheduling-phase.h"

#include "src/compiler/turboshaft/copying-phase.h"
#include "src/compiler/turboshaft/machine-optimization-reducer.h"
#include "src/handles/handles-inl.h"
#include "src/numbers/conversions-inl.h"
#include "src/objects/objects-inl.h"

namespace v8::internal::compiler::turboshaft {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

template <class Next>
class ReschedulingReducer : public Next {
 public:
  TURBOSHAFT_REDUCER_BOILERPLATE(Rescheduling)

  V<None> AssembleOutputGraphCheckException(const CheckExceptionOp& op) {
    std::optional<CatchScope> catch_scope;
    if (op.catch_block != nullptr) {
      catch_scope.emplace(Asm(), this->MapToNewGraph(op.catch_block));
    }
#if V8_ENABLE_WEBASSEMBLY
    if (!op.effect_handlers.empty()) {
      base::Vector<EffectHandler> output_handlers =
          __ graph_zone() -> template AllocateVector<EffectHandler>(
              op.effect_handlers.size());
      for (int i = 0; i < op.effect_handlers.length(); ++i) {
        output_handlers[i].tag_and_kind = op.effect_handlers[i].tag_and_kind;
        output_handlers[i].sig = op.effect_handlers[i].sig;
        if (!op.effect_handlers[i].is_switch()) {
          output_handlers[i].block =
              this->MapToNewGraph(op.effect_handlers[i].block);
        } else {
          output_handlers[i].block = nullptr;
        }
      }
      __ set_effect_handlers_for_next_call(output_handlers);
    }
#endif

    // We only inline the DidntThrowOp (which triggers the actual
    // reduction/emission of the throwing operation), but we do not inline the
    // rest of the success block here. Inlining the rest of the success block
    // inline would cause its operations to evade rescheduling. Instead, we emit
    // a Goto to the mapped success block, allowing its operations to be
    // properly scheduled when the rescheduler visits that block later. Note
    // that this will result in the DidntThrowOp being visited twice (once here,
    // and once when the success block is processed normally). This is safe
    // because the rescheduler's `VisitOpAndUpdateMapping` will see that it has
    // already been mapped and return early without emitting it again.
    Graph::OpIndexIterator it(op.didnt_throw_block->begin(), &graph_);
    CHECK(graph_.Get(*it).template Is<DidntThrowOp>());
    if (!__ InlineOp(*it, op.didnt_throw_block)) {
      __ clear_effect_handlers();
      return V<None>::Invalid();
    }
    __ clear_effect_handlers();

    Block* successor_block = this->MapToNewGraph(op.didnt_throw_block);
    if V8_UNLIKELY (trace_scheduling_) {
      std::cout << "CheckException: successor_block="
                << successor_block->index()
                << " predecessor_count=" << successor_block->PredecessorCount()
                << "\n";
    }
    __ Goto(successor_block);
    if V8_UNLIKELY (trace_scheduling_) {
      std::cout << "CheckException: after Goto, predecessor_count="
                << successor_block->PredecessorCount() << "\n";
    }

    return V<None>::Invalid();
  }

  // We want to prevent automatically inlining blocks, because those blocks
  // would evade the rescheduling.
  bool CanAutoInlineBlocksWithSinglePredecessor() const { return false; }

 public:
  const Graph& graph_ = Asm().input_graph();
  bool trace_scheduling_ = false;
};

class RandomRescheduler : public Assembler<ReschedulingReducer, GraphVisitor> {
  using Base = Assembler<ReschedulingReducer, GraphVisitor>;
  enum class OpState {
    kPending,  // This operation is not yet emitted and not yet ready to do so.
    kReady,    // This operation is not yet emitted but is ready to be emitted.
    kFixed,    // This operation has a fixed location and will be handled
               // specially (e.g. block terminators and Phis).
    kScheduled,  // This operation has been emitted.
  };

 public:
  RandomRescheduler(PipelineData* data, Graph& input_graph, Graph& output_graph,
                    Zone* phase_zone)
      : Base(data, input_graph, output_graph, phase_zone),
        op_states_(input_graph.op_id_count(), OpState::kPending, phase_zone,
                   &input_graph),
        ready_ops_(phase_zone),
        rng_(v8_flags.random_seed ^ input_graph.op_id_count()) {}

  void Run() {
    Analyze();

    // Creating initial old-to-new Block mapping.
    for (const Block& input_block : graph_.blocks()) {
      block_mapping_[input_block.index()] = __ output_graph().NewBlock(
          input_block.IsLoop() ? Block::Kind::kLoopHeader : Block::Kind::kMerge,
          &input_block);

      // Set Parameters, Phis and CatchBlockBegin to fixed.
      for (OpIndex index : graph_.OperationIndices(input_block)) {
        const Operation& op = graph_.Get(index);
        if (op.template Is<ParameterOp>() || op.template Is<PhiOp>() ||
            op.template Is<CatchBlockBeginOp>()) {
          op_states_[index] = OpState::kFixed;
        }
      }

      // Set block terminators to fixed.
      const Operation& last_op = input_block.LastOperation(graph_);
      CHECK(IsBlockTerminator(last_op.opcode));
      op_states_[graph_.Index(last_op)] = OpState::kFixed;
    }

    // Visiting the graph.
    VisitAllBlocks();

#ifdef DEBUG
    for (OpIndex index : graph_.AllOperationIndices()) {
      if (op_states_[index] != OpState::kScheduled) {
        std::stringstream stream;
        stream << index.id() << ": " << graph_.Get(index);
        FATAL("Operation NOT scheduled: %s, state: %d", stream.str().c_str(),
              static_cast<int>(op_states_[index]));
      }
    }
#endif

    Finalize();
  }

  void VisitAllBlocks() {
    base::SmallVector<const Block*, 128> visit_stack;

    visit_stack.push_back(&graph_.StartBlock());
    while (!visit_stack.empty()) {
      const Block* block = visit_stack.back();
      visit_stack.pop_back();
      VisitBlock(block);

      for (Block* child = block->LastChild(); child != nullptr;
           child = child->NeighboringChild()) {
        visit_stack.push_back(child);
      }
    }
  }

  void VisitBlock(const Block* input_block) {
    if (tick_counter_) tick_counter_->TickAndMaybeEnterSafepoint();
    __ SetCurrentOrigin(OpIndex::Invalid());

    Block* new_block = MapToNewGraph(input_block);
    __ BindReachable(new_block);
    if V8_UNLIKELY (trace_scheduling_) {
      std::cout << "VisitBlock: input " << input_block->index() << "\n";
    }
    VisitBlockBody(input_block);
  }

  void VisitBlockBody(const Block* input_block,
                      int added_block_phi_input = -1) {
    CHECK_NOT_NULL(__ current_block());
    current_input_block_ = input_block;

    if V8_UNLIKELY (trace_scheduling_) {
      std::cout << "=== Processing block " << input_block->index() << "===\n";
    }

    // Entry block contains parameters and they need to stay in place.
    if (input_block->index() == graph_.StartBlock().index()) {
      // Nothing to do for the entry block. The parameters are already in place.
      for (OpIndex index : graph_.OperationIndices(*input_block)) {
        if (graph_.Get(index).template Is<ParameterOp>()) {
          CHECK_EQ(op_states_[index], OpState::kFixed);
          VisitOpAndUpdateMapping(index, input_block);
          op_states_[index] = OpState::kScheduled;
          if V8_UNLIKELY (trace_scheduling_) {
            std::cout << std::setw(4) << index.id() << ":" << graph_.Get(index)
                      << " (because Parameter)\n";
          }
        }
      }
    }

    // Visit CatchBlockBeginOp if present.
    Graph::OpIndexIterator it(input_block->begin(), &graph_);
    if (graph_.Get(*it).template Is<CatchBlockBeginOp>()) {
      CHECK_EQ(op_states_[*it], OpState::kFixed);
      VisitOpAndUpdateMapping(*it, input_block);
      op_states_[*it] = OpState::kScheduled;
      if V8_UNLIKELY (trace_scheduling_) {
        std::cout << std::setw(4) << (*it).id() << ":" << graph_.Get(*it)
                  << " (because CatchBlockBegin)\n";
      }
    }

    // Visiting Phis.
    for (OpIndex index : graph_.OperationIndices(*input_block)) {
      CHECK_NOT_NULL(__ current_block());
      if (graph_.Get(index).template Is<PhiOp>()) {
        CHECK_EQ(op_states_[index], OpState::kFixed);
        VisitOpAndUpdateMapping(index, input_block);
        op_states_[index] = OpState::kScheduled;
        if V8_UNLIKELY (trace_scheduling_) {
          std::cout << std::setw(4) << index.id() << ":" << graph_.Get(index)
                    << " (because Phi)\n";
        }
      }
    }

    // Now visit and schedule everything else.
    Graph::OpIndexIterator first_unscheduled_iter =
        graph_.OperationIndices(*input_block).begin();

    while (true) {
      ComputeReadyOperations(input_block, first_unscheduled_iter);
      OpIndex next_op = PickNextOperation();
      if (!next_op.valid()) break;

      CHECK_EQ(op_states_[next_op], OpState::kReady);

      VisitOpAndUpdateMapping(next_op, input_block);
      op_states_[next_op] = OpState::kScheduled;

      if V8_UNLIKELY (trace_scheduling_) {
        std::cout << std::setw(4) << next_op.id() << ":" << graph_.Get(next_op)
                  << " out of {";
        for (OpIndex ready_op : ready_ops_) {
          std::cout << ready_op.id() << ", ";
        }
        std::cout << "}\n";
      }
    }

    // The final operation (which should be a block terminator) of the block
    // is processed separately, because it is generally fixed.
    const Operation& terminator = input_block->LastOperation(graph_);
    CHECK(terminator.IsBlockTerminator());
    OpIndex terminator_index = graph_.Index(terminator);
    CHECK_EQ(op_states_[terminator_index], OpState::kFixed);
    VisitBlockTerminator<false>(terminator, input_block);
    op_states_[terminator_index] = OpState::kScheduled;

    if V8_UNLIKELY (trace_scheduling_) {
      std::cout << std::setw(4) << terminator_index.id() << ":" << terminator
                << " (because block terminator)\n";
    }
  }

  bool VisitOpAndUpdateMapping(OpIndex index, const Block* input_block) {
    if (__ current_block() == nullptr) return false;

    // DidntThrowOp is emitted already during processing of CheckException, so
    // we need to prevent it being processed again here.
    if (V8_UNLIKELY(op_mapping_[index].valid())) {
      CHECK(graph_.Get(index).Is<DidntThrowOp>());
      return true;
    }
    return Base::VisitOpAndUpdateMapping<false>(index, input_block);
  }

  OpIndex PickNextOperation() {
    size_t ready_op_count = ready_ops_.size();
    if (ready_op_count == 0) return OpIndex::Invalid();

    size_t pick = static_cast<size_t>(rng_.NextInt64()) % ready_op_count;
    auto it = ready_ops_.begin();
    std::advance(it, pick);
    return *it;
  }

  bool AllInputsScheduled(OpIndex index) {
    for (OpIndex input : graph_.Get(index).inputs()) {
      if (op_states_[input] != OpState::kScheduled) return false;
    }
    return true;
  }

  bool IsReady(OpIndex index, const Operation& op,
               EffectDimensions::Bits produced_effects) {
    // To be ready for scheduling, the operation has to satisfy two
    // requirements: 1.) None of the effects currently walked over during block
    // iteration does prevent this operation to flow up (because none of the
    // produced effects is consumed by this operation). 2.) All its inputs must
    // have been scheduled.
    auto consumed_effects = op.Effects().consumes.bits();
    if ((consumed_effects & produced_effects) != 0) return false;
    return AllInputsScheduled(index);
  }

  void ComputeReadyOperations(const Block* input_block,
                              Graph::OpIndexIterator& first_unscheduled_iter) {
    ready_ops_.clear();

    Graph::OpIndexIterator block_end =
        graph_.OperationIndices(*input_block).end();

    // When considering the next operation to schedule, this is the sum of all
    // effects that we have seen from the current position up to that operation,
    // so this is the effects that the operation would observe in the input
    // schedule.
    EffectDimensions::Bits produced_effects = 0;

    // Whether we found at least one new unscheduled operation.
    bool has_advanced = false;

    for (Graph::OpIndexIterator iter = first_unscheduled_iter;
         iter != block_end; ++iter) {
      OpIndex index = *iter;
      const Operation& op = graph_.Get(index);
      switch (op_states_[index]) {
        case OpState::kPending:
          if (!IsReady(index, op, produced_effects)) {
            break;
          }
          op_states_[index] = OpState::kReady;
          [[fallthrough]];
        case OpState::kReady:
          // If this was ready before, it should still be.
          CHECK(IsReady(index, op, produced_effects));
          ready_ops_.insert(index);
          if (!has_advanced) {
            has_advanced = true;
            first_unscheduled_iter = iter;
          }
          if (ready_ops_.size() == 30) return;
          break;
        case OpState::kFixed:
          // This operation has a fixed position and cannot be rescheduled, but
          // we need to record its effect.
          break;
        case OpState::kScheduled:
          // This operation is already scheduled and we can ignore it.
          continue;
      }
      // Record this operation's produced effects, so that following
      // operations will prevented from flowing up if they consume any of
      // them.
      produced_effects |= op.Effects().produces.bits();
    }
  }

  FixedOpIndexSidetable<OpState> op_states_;
  ZoneAbslFlatHashSet<OpIndex> ready_ops_;
  base::RandomNumberGenerator rng_;
};

void RandomReschedulingPhase::Run(PipelineData* data, Zone* temp_zone) {
  Graph& input_graph = data->graph();
  RandomRescheduler rescheduler(data, input_graph,
                                input_graph.GetOrCreateCompanion(), temp_zone);
  rescheduler.Run();
}

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::compiler::turboshaft
