/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

namespace antlr4 {

/**
 * Useful for rewriting out a buffered input token stream after doing some
 * augmentation or other manipulations on it.
 *
 * <p>
 * You can insert stuff, replace, and delete chunks. Note that the operations
 * are done lazily--only if you convert the buffer to a {@link String} with
 * {@link TokenStream#getText()}. This is very efficient because you are not
 * moving data around all the time. As the buffer of tokens is converted to
 * strings, the {@link #getText()} method(s) scan the input token stream and
 * check to see if there is an operation at the current index. If so, the
 * operation is done and then normal {@link String} rendering continues on the
 * buffer. This is like having multiple Turing machine instruction streams
 * (programs) operating on a single input tape. :)</p>
 *
 * <p>
 * This rewriter makes no modifications to the token stream. It does not ask the
 * stream to fill itself up nor does it advance the input cursor. The token
 * stream {@link TokenStream#index()} will return the same value before and
 * after any {@link #getText()} call.</p>
 *
 * <p>
 * The rewriter only works on tokens that you have in the buffer and ignores the
 * current input cursor. If you are buffering tokens on-demand, calling
 * {@link #getText()} halfway through the input will only do rewrites for those
 * tokens in the first half of the file.</p>
 *
 * <p>
 * Since the operations are done lazily at {@link #getText}-time, operations do
 * not screw up the token index values. That is, an insert operation at token
 * index {@code i} does not change the index values for tokens
 * {@code i}+1..n-1.</p>
 *
 * <p>
 * Because operations never actually alter the buffer, you may always get the
 * original token stream back without undoing anything. Since the instructions
 * are queued up, you can easily simulate transactions and roll back any changes
 * if there is an error just by removing instructions. For example,</p>
 *
 * <pre>
 * CharStream input = new ANTLRFileStream("input");
 * TLexer lex = new TLexer(input);
 * CommonTokenStream tokens = new CommonTokenStream(lex);
 * T parser = new T(tokens);
 * TokenStreamRewriter rewriter = new TokenStreamRewriter(tokens);
 * parser.startRule();
 * </pre>
 *
 * <p>
 * Then in the rules, you can execute (assuming rewriter is visible):</p>
 *
 * <pre>
 * Token t,u;
 * ...
 * rewriter.insertAfter(t, "text to put after t");}
 * rewriter.insertAfter(u, "text after u");}
 * System.out.println(rewriter.getText());
 * </pre>
 *
 * <p>
 * You can also have multiple "instruction streams" and get multiple rewrites
 * from a single pass over the input. Just name the instruction streams and use
 * that name again when printing the buffer. This could be useful for generating
 * a C file and also its header file--all from the same buffer:</p>
 *
 * <pre>
 * rewriter.insertAfter("pass1", t, "text to put after t");}
 * rewriter.insertAfter("pass2", u, "text after u");}
 * System.out.println(rewriter.getText("pass1"));
 * System.out.println(rewriter.getText("pass2"));
 * </pre>
 *
 * <p>
 * If you don't use named rewrite streams, a "default" stream is used as the
 * first example shows.</p>
 */
class ANTLR4CPP_PUBLIC TokenStreamRewriter {
 public:
  static const std::string DEFAULT_PROGRAM_NAME;
  static const size_t PROGRAM_INIT_SIZE = 100;
  static const size_t MIN_TOKEN_INDEX = 0;

  TokenStreamRewriter(TokenStream* tokens);
  virtual ~TokenStreamRewriter();

  TokenStream* getTokenStream();

  virtual void rollback(size_t instructionIndex);

  /// Rollback the instruction stream for a program so that
  /// the indicated instruction (via instructionIndex) is no
  /// longer in the stream.  UNTESTED!
  virtual void rollback(const std::string& programName,
                        size_t instructionIndex);

  virtual void deleteProgram();

  /// Reset the program so that no instructions exist.
  virtual void deleteProgram(const std::string& programName);
  virtual void insertAfter(Token* t, const std::string& text);
  virtual void insertAfter(size_t index, const std::string& text);
  virtual void insertAfter(const std::string& programName, Token* t,
                           const std::string& text);
  virtual void insertAfter(const std::string& programName, size_t index,
                           const std::string& text);

  virtual void insertBefore(Token* t, const std::string& text);
  virtual void insertBefore(size_t index, const std::string& text);
  virtual void insertBefore(const std::string& programName, Token* t,
                            const std::string& text);
  virtual void insertBefore(const std::string& programName, size_t index,
                            const std::string& text);

  virtual void replace(size_t index, const std::string& text);
  virtual void replace(size_t from, size_t to, const std::string& text);
  virtual void replace(Token* indexT, const std::string& text);
  virtual void replace(Token* from, Token* to, const std::string& text);
  virtual void replace(const std::string& programName, size_t from, size_t to,
                       const std::string& text);
  virtual void replace(const std::string& programName, Token* from, Token* to,
                       const std::string& text);

  virtual void Delete(size_t index);
  virtual void Delete(size_t from, size_t to);
  virtual void Delete(Token* indexT);
  virtual void Delete(Token* from, Token* to);
  virtual void Delete(const std::string& programName, size_t from, size_t to);
  virtual void Delete(const std::string& programName, Token* from, Token* to);

  virtual size_t getLastRewriteTokenIndex();

  /// Return the text from the original tokens altered per the
  /// instructions given to this rewriter.
  virtual std::string getText();

  /** Return the text from the original tokens altered per the
   *  instructions given to this rewriter in programName.
   */
  std::string getText(std::string programName);

  /// Return the text associated with the tokens in the interval from the
  /// original token stream but with the alterations given to this rewriter.
  /// The interval refers to the indexes in the original token stream.
  /// We do not alter the token stream in any way, so the indexes
  /// and intervals are still consistent. Includes any operations done
  /// to the first and last token in the interval. So, if you did an
  /// insertBefore on the first token, you would get that insertion.
  /// The same is true if you do an insertAfter the stop token.
  virtual std::string getText(const misc::Interval& interval);

  virtual std::string getText(const std::string& programName,
                              const misc::Interval& interval);

 protected:
  class RewriteOperation {
   public:
    /// What index into rewrites List are we?
    size_t index;
    std::string text;

    /// Token buffer index.
    size_t instructionIndex;

    RewriteOperation(TokenStreamRewriter* outerInstance, size_t index);
    RewriteOperation(TokenStreamRewriter* outerInstance, size_t index,
                     const std::string& text);
    virtual ~RewriteOperation();

    /// Execute the rewrite operation by possibly adding to the buffer.
    /// Return the index of the next token to operate on.

    virtual size_t execute(std::string* buf);
    virtual std::string toString();

   private:
    TokenStreamRewriter* const outerInstance;
    void InitializeInstanceFields();
  };

  class InsertBeforeOp : public RewriteOperation {
   private:
    TokenStreamRewriter* const outerInstance;

   public:
    InsertBeforeOp(TokenStreamRewriter* outerInstance, size_t index,
                   const std::string& text);

    virtual size_t execute(std::string* buf) override;
  };

  class ReplaceOp : public RewriteOperation {
   private:
    TokenStreamRewriter* const outerInstance;

   public:
    size_t lastIndex;

    ReplaceOp(TokenStreamRewriter* outerInstance, size_t from, size_t to,
              const std::string& text);
    virtual size_t execute(std::string* buf) override;
    virtual std::string toString() override;

   private:
    void InitializeInstanceFields();
  };

  /// Our source stream
  TokenStream* const tokens;

  /// You may have multiple, named streams of rewrite operations.
  /// I'm calling these things "programs."
  /// Maps String (name) -> rewrite (List)
  std::map<std::string, std::vector<RewriteOperation*>> _programs;

  /// <summary>
  /// Map String (program name) -> Integer index </summary>
  std::map<std::string, size_t> _lastRewriteTokenIndexes;
  virtual size_t getLastRewriteTokenIndex(const std::string& programName);
  virtual void setLastRewriteTokenIndex(const std::string& programName,
                                        size_t i);
  virtual std::vector<RewriteOperation*>& getProgram(const std::string& name);

  /// <summary>
  /// We need to combine operations and report invalid operations (like
  ///  overlapping replaces that are not completed nested).  Inserts to
  ///  same index need to be combined etc...   Here are the cases:
  ///
  ///  I.i.u I.j.v                                leave alone, nonoverlapping
  ///  I.i.u I.i.v                                combine: Iivu
  ///
  ///  R.i-j.u R.x-y.v    | i-j in x-y            delete first R
  ///  R.i-j.u R.i-j.v                            delete first R
  ///  R.i-j.u R.x-y.v    | x-y in i-j            ERROR
  ///  R.i-j.u R.x-y.v    | boundaries overlap    ERROR
  ///
  ///  Delete special case of replace (text==null):
  ///  D.i-j.u D.x-y.v    | boundaries overlap    combine to
  ///  max(min)..max(right)
  ///
  ///  I.i.u R.x-y.v | i in (x+1)-y           delete I (since insert before
  ///                                         we're not deleting i)
  ///  I.i.u R.x-y.v | i not in (x+1)-y       leave alone, nonoverlapping
  ///  R.x-y.v I.i.u | i in x-y               ERROR
  ///  R.x-y.v I.x.u                          R.x-y.uv (combine, delete I)
  ///  R.x-y.v I.i.u | i not in x-y           leave alone, nonoverlapping
  ///
  ///  I.i.u = insert u before op @ index i
  ///  R.x-y.u = replace x-y indexed tokens with u
  ///
  ///  First we need to examine replaces.  For any replace op:
  ///
  ///         1. wipe out any insertions before op within that range.
  ///     2. Drop any replace op before that is contained completely within
  ///         that range.
  ///     3. Throw exception upon boundary overlap with any previous replace.
  ///
  ///  Then we can deal with inserts:
  ///
  ///         1. for any inserts to same index, combine even if not adjacent.
  ///         2. for any prior replace with same left boundary, combine this
  ///         insert with replace and delete this replace.
  ///         3. throw exception if index in same range as previous replace
  ///
  ///  Don't actually delete; make op null in list. Easier to walk list.
  ///  Later we can throw as we add to index -> op map.
  ///
  ///  Note that I.2 R.2-2 will wipe out I.2 even though, technically, the
  ///  inserted stuff would be before the replace range.  But, if you
  ///  add tokens in front of a method body '{' and then delete the method
  ///  body, I think the stuff before the '{' you added should disappear too.
  ///
  ///  Return a map from token index to operation.
  /// </summary>
  virtual std::unordered_map<size_t, RewriteOperation*>
  reduceToSingleOperationPerIndex(std::vector<RewriteOperation*>& rewrites);

  virtual std::string catOpText(std::string* a, std::string* b);

  /// Get all operations before an index of a particular kind.
  template <typename T>
  std::vector<T*> getKindOfOps(std::vector<RewriteOperation*> rewrites,
                               size_t before) {
    std::vector<T*> ops;
    for (size_t i = 0; i < before && i < rewrites.size(); i++) {
      T* op = dynamic_cast<T*>(rewrites[i]);
      if (op == nullptr) {  // ignore deleted or non matching entries
        continue;
      }
      ops.push_back(op);
    }
    return ops;
  }

 private:
  std::vector<RewriteOperation*>& initializeProgram(const std::string& name);
};

}  // namespace antlr4
