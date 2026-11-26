// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "query.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <queue>

#include "llvm/Support/JSON.h"

namespace dfg {

namespace {

int64_t SatAdd(int64_t a, int64_t b) {
  int64_t res;
  if (!__builtin_add_overflow(a, b, &res)) return res;
  return a >= 0 ? std::numeric_limits<int64_t>::max()
                : std::numeric_limits<int64_t>::min();
}

int64_t SatSub(int64_t a, int64_t b) {
  int64_t res;
  if (!__builtin_sub_overflow(a, b, &res)) return res;
  return a >= 0 ? std::numeric_limits<int64_t>::max()
                : std::numeric_limits<int64_t>::min();
}

int64_t SatMul(int64_t a, int64_t b) {
  int64_t res;
  if (!__builtin_mul_overflow(a, b, &res)) return res;
  bool neg = (a < 0) != (b < 0);
  return neg ? std::numeric_limits<int64_t>::min()
             : std::numeric_limits<int64_t>::max();
}

int64_t SatNeg(int64_t a) {
  if (a == std::numeric_limits<int64_t>::min()) {
    return std::numeric_limits<int64_t>::max();
  }
  return -a;
}

int64_t SafeDiv(int64_t a, int64_t b) {
  if (b == 0) return 0;
  if (a == std::numeric_limits<int64_t>::min() && b == -1) {
    return std::numeric_limits<int64_t>::max();
  }
  return a / b;
}

int64_t SatShl(int64_t a, int64_t shift) {
  if (shift <= 0 || a == 0) return a;
  if (shift >= 63) {
    return a > 0 ? std::numeric_limits<int64_t>::max()
                 : std::numeric_limits<int64_t>::min();
  }
  if (a > 0) {
    if (a > (std::numeric_limits<int64_t>::max() >> shift)) {
      return std::numeric_limits<int64_t>::max();
    }
    return a << shift;
  }
  if (a < (std::numeric_limits<int64_t>::min() / (int64_t{1} << shift))) {
    return std::numeric_limits<int64_t>::min();
  }
  return static_cast<int64_t>(static_cast<uint64_t>(a) << shift);
}

std::string NormalizeIntegralType(std::string S) {
  // Strip qualifiers and reference tokens
  auto StripWord = [&](const std::string& W) {
    size_t pos = 0;
    while ((pos = S.find(W, pos)) != std::string::npos) {
      bool left_ok =
          (pos == 0 || !std::isalnum(static_cast<unsigned char>(S[pos - 1])));
      size_t end = pos + W.size();
      bool right_ok = (end == S.size() ||
                       !std::isalnum(static_cast<unsigned char>(S[end])));
      if (left_ok && right_ok) {
        S.erase(pos, W.size());
      } else {
        pos = end;
      }
    }
  };
  StripWord("const");
  StripWord("volatile");
  StripWord("constexpr");
  StripWord("class");
  StripWord("struct");
  StripWord("enum");
  S.erase(std::remove(S.begin(), S.end(), '&'), S.end());

  // Collapse whitespace and trim
  std::string Out;
  bool InSpace = false;
  for (char c : S) {
    if (std::isspace(static_cast<unsigned char>(c))) {
      if (!Out.empty()) InSpace = true;
    } else {
      if (InSpace) {
        Out.push_back(' ');
        InSpace = false;
      }
      Out.push_back(c);
    }
  }
  return Out;
}

ValueRange LookupCxxIntegralTypeRange(const std::string& RawType) {
  if (RawType.empty()) return ValueRange::Unknown();
  // Unwrap atomic<T> / std::atomic<T> / atomic_ref<T> / AtomicValue<T>
  std::string Unwrapped = RawType;
  for (int iter = 0; iter < 2; ++iter) {
    size_t lt = Unwrapped.find('<');
    size_t gt = Unwrapped.rfind('>');
    if (lt != std::string::npos && gt != std::string::npos && gt > lt) {
      std::string Outer = NormalizeIntegralType(Unwrapped.substr(0, lt));
      if (Outer == "std::atomic" || Outer == "atomic" ||
          Outer == "std::atomic_ref" || Outer == "atomic_ref" ||
          Outer == "v8::base::AtomicValue" || Outer == "base::AtomicValue" ||
          Outer == "AtomicValue") {
        Unwrapped = Unwrapped.substr(lt + 1, gt - lt - 1);
        continue;
      }
    }
    break;
  }
  // Exclude pointers, arrays, functions, and template types.
  if (Unwrapped.find('*') != std::string::npos ||
      Unwrapped.find('(') != std::string::npos ||
      Unwrapped.find(')') != std::string::npos ||
      Unwrapped.find('[') != std::string::npos ||
      Unwrapped.find('<') != std::string::npos) {
    return ValueRange::Unknown();
  }
  std::string T = NormalizeIntegralType(Unwrapped);
  if (T.empty()) return ValueRange::Unknown();

  auto MatchName = [](const std::string& Name) -> ValueRange {
    if (Name == "bool" || Name == "_Bool") {
      return ValueRange::Interval(0, 1);
    }
    if (Name == "uint8_t" || Name == "unsigned char" || Name == "byte") {
      return ValueRange::Interval(0, 255);
    }
    if (Name == "int8_t" || Name == "signed char" || Name == "char") {
      return ValueRange::Interval(-128, 127);
    }
    if (Name == "uint16_t" || Name == "unsigned short" ||
        Name == "unsigned short int" || Name == "char16_t") {
      return ValueRange::Interval(0, 65535);
    }
    if (Name == "int16_t" || Name == "short" || Name == "short int" ||
        Name == "signed short" || Name == "signed short int") {
      return ValueRange::Interval(-32768, 32767);
    }
    if (Name == "uint32_t" || Name == "unsigned int" || Name == "unsigned" ||
        Name == "char32_t") {
      return ValueRange::Interval(0, 4294967295LL);
    }
    if (Name == "int32_t" || Name == "int" || Name == "signed int" ||
        Name == "signed" || Name == "wchar_t") {
      return ValueRange::Interval(-2147483648LL, 2147483647LL);
    }
    if (Name == "uint64_t" || Name == "unsigned long" ||
        Name == "unsigned long int" || Name == "unsigned long long" ||
        Name == "unsigned long long int" || Name == "size_t" ||
        Name == "uintptr_t" || Name == "Address") {
      return ValueRange::Interval(0, std::numeric_limits<int64_t>::max());
    }
    if (Name == "int64_t" || Name == "long" || Name == "long int" ||
        Name == "signed long" || Name == "signed long int" ||
        Name == "long long" || Name == "long long int" ||
        Name == "signed long long" || Name == "signed long long int" ||
        Name == "ptrdiff_t" || Name == "intptr_t" || Name == "ssize_t") {
      return ValueRange::Interval(std::numeric_limits<int64_t>::min(),
                                  std::numeric_limits<int64_t>::max());
    }
    return ValueRange::Unknown();
  };

  ValueRange R = MatchName(T);
  if (R.HasValue) return R;
  size_t scope_pos = T.rfind("::");
  if (scope_pos != std::string::npos) {
    return MatchName(T.substr(scope_pos + 2));
  }
  return ValueRange::Unknown();
}

HeapTaint GetIntrinsicHeapTaint(const Node* N) {
  if (!N) return HeapTaint::None;
  if (N->Type.rfind(" [heap:sandbox]") != std::string::npos) {
    return HeapTaint::InSandbox;
  }
  if (N->Type.rfind(" [heap:embedder]") != std::string::npos) {
    return HeapTaint::Embedder;
  }
  if (N->Type.rfind(" [heap:trusted]") != std::string::npos) {
    return HeapTaint::TrustedSpace;
  }
  return HeapTaint::None;
}

std::string StripHeapTaintSuffix(std::string S) {
  for (const char* Suffix :
       {" [heap:sandbox]", " [heap:embedder]", " [heap:trusted]"}) {
    size_t pos = S.rfind(Suffix);
    if (pos != std::string::npos) {
      S.erase(pos, std::strlen(Suffix));
    }
  }
  size_t lb_pos = S.rfind(" [loop_bound:");
  if (lb_pos != std::string::npos) {
    size_t end_pos = S.find(']', lb_pos);
    if (end_pos != std::string::npos) {
      S.erase(lb_pos, end_pos - lb_pos + 1);
    }
  }
  return S;
}

bool GetIntrinsicLoopBound(const Node* N, int64_t& OutBound) {
  if (!N) return false;
  size_t pos = N->Type.rfind(" [loop_bound:");
  if (pos == std::string::npos) return false;
  const char* start = N->Type.c_str() + pos + 13;
  char* endptr = nullptr;
  errno = 0;
  long long val = std::strtoll(start, &endptr, 10);
  if (endptr != start && *endptr == ']' && errno == 0) {
    OutBound = static_cast<int64_t>(val);
    return true;
  }
  return false;
}

ValueRange GetIntrinsicRange(const Node* N) {
  if (!N) return ValueRange::Unknown();
  std::string T = StripHeapTaintSuffix(N->Type);

  // 1. Check compile-time constant suffix " = <val>"
  size_t eq_pos = T.rfind(" = ");
  if (eq_pos != std::string::npos) {
    std::string ValStr = T.substr(eq_pos + 3);
    char* endptr = nullptr;
    errno = 0;
    long long val = std::strtoll(ValStr.c_str(), &endptr, 10);
    if (endptr != ValStr.c_str() && errno == 0) {
      return ValueRange::Exact(static_cast<int64_t>(val));
    }
  }

  // 2. Check Literal node name directly
  if (N->Kind == NodeKind::Literal) {
    if (N->Name == "true") return ValueRange::Exact(1);
    if (N->Name == "false") return ValueRange::Exact(0);
    if (!N->Name.empty() &&
        (std::isdigit(static_cast<unsigned char>(N->Name[0])) ||
         N->Name[0] == '-' || N->Name[0] == '+')) {
      char* endptr = nullptr;
      errno = 0;
      long long val = std::strtoll(N->Name.c_str(), &endptr, 0);
      if (endptr != N->Name.c_str() && errno == 0) {
        return ValueRange::Exact(static_cast<int64_t>(val));
      }
    }
  }

  // 3. Check bitfield width suffix " : <width>"
  std::string BaseType =
      (eq_pos != std::string::npos) ? T.substr(0, eq_pos) : T;
  size_t colon_pos = BaseType.rfind(" : ");
  if (colon_pos != std::string::npos) {
    std::string WidthStr = BaseType.substr(colon_pos + 3);
    char* endptr = nullptr;
    long width = std::strtol(WidthStr.c_str(), &endptr, 10);
    if (endptr != WidthStr.c_str() && width > 0 && width <= 63) {
      int64_t max_val = (width == 63)
                            ? std::numeric_limits<int64_t>::max()
                            : static_cast<int64_t>((1ULL << width) - 1);
      return ValueRange::Interval(0, max_val);
    }
    BaseType = BaseType.substr(0, colon_pos);
  }

  // 4. Fallback to C++ integral type bounds
  return LookupCxxIntegralTypeRange(BaseType);
}

ValueRange ClampRangeToType(const ValueRange& R, const ValueRange& TypeR) {
  if (!R.HasValue) return TypeR;
  if (!TypeR.HasValue) return R;
  int64_t lo = std::max(R.Min, TypeR.Min);
  int64_t hi = std::min(R.Max, TypeR.Max);
  if (lo <= hi) {
    return ValueRange::Interval(lo, hi);
  }
  return TypeR;
}

bool IsUnaryOpName(const std::string& Name) {
  return Name == "-" || Name == "~" || Name == "!" || Name == "++" ||
         Name == "--";
}

bool IsBinaryOpName(const std::string& Name) {
  return Name == "+" || Name == "-" || Name == "*" || Name == "/" ||
         Name == "%" || Name == "<<" || Name == ">>" || Name == "&" ||
         Name == "|" || Name == "^" || Name == "min()" || Name == "max()";
}

bool IsOperatorExprName(const std::string& Name) {
  return IsUnaryOpName(Name) || IsBinaryOpName(Name);
}

ValueRange EvalUnaryOp(const std::string& Op, const ValueRange& A,
                       const ValueRange& TypeR) {
  if (!A.HasValue) return TypeR;
  if (Op == "++") {
    return ClampRangeToType(
        ValueRange::Interval(SatAdd(A.Min, 1), SatAdd(A.Max, 1)), TypeR);
  }
  if (Op == "--") {
    return ClampRangeToType(
        ValueRange::Interval(SatSub(A.Min, 1), SatSub(A.Max, 1)), TypeR);
  }
  if (Op == "-") {
    return ClampRangeToType(
        ValueRange::Interval(SatNeg(A.Max), SatNeg(A.Min)), TypeR);
  }
  if (Op == "~") {
    return ClampRangeToType(ValueRange::Interval(~A.Max, ~A.Min), TypeR);
  }
  if (Op == "!") {
    if (A.Min > 0 || A.Max < 0) return ValueRange::Exact(0);
    if (A.Min == 0 && A.Max == 0) return ValueRange::Exact(1);
    return ValueRange::Interval(0, 1);
  }
  return TypeR;
}

ValueRange EvalBinaryOp(const std::string& Op, const ValueRange& A,
                        const ValueRange& B, const ValueRange& TypeR) {
  if (Op == "&") {
    if (A.HasValue && B.HasValue) {
      if (A.IsConstant() && B.IsConstant()) {
        return ClampRangeToType(ValueRange::Exact(A.Min & B.Min), TypeR);
      }
      if (A.Min >= 0 && B.Min >= 0) {
        return ClampRangeToType(
            ValueRange::Interval(0, std::min(A.Max, B.Max)), TypeR);
      }
      if (A.Min >= 0) {
        return ClampRangeToType(ValueRange::Interval(0, A.Max), TypeR);
      }
      if (B.Min >= 0) {
        return ClampRangeToType(ValueRange::Interval(0, B.Max), TypeR);
      }
    } else if (A.HasValue && A.Min >= 0) {
      return ClampRangeToType(ValueRange::Interval(0, A.Max), TypeR);
    } else if (B.HasValue && B.Min >= 0) {
      return ClampRangeToType(ValueRange::Interval(0, B.Max), TypeR);
    }
    return TypeR;
  }

  if (!A.HasValue || !B.HasValue) return TypeR;

  if (Op == "min()") {
    return ClampRangeToType(
        ValueRange::Interval(std::min(A.Min, B.Min), std::min(A.Max, B.Max)),
        TypeR);
  }
  if (Op == "max()") {
    return ClampRangeToType(
        ValueRange::Interval(std::max(A.Min, B.Min), std::max(A.Max, B.Max)),
        TypeR);
  }
  if (Op == "+") {
    return ClampRangeToType(
        ValueRange::Interval(SatAdd(A.Min, B.Min), SatAdd(A.Max, B.Max)),
        TypeR);
  }
  if (Op == "-") {
    return ClampRangeToType(
        ValueRange::Interval(SatSub(A.Min, B.Max), SatSub(A.Max, B.Min)),
        TypeR);
  }
  if (Op == "*") {
    int64_t p1 = SatMul(A.Min, B.Min);
    int64_t p2 = SatMul(A.Min, B.Max);
    int64_t p3 = SatMul(A.Max, B.Min);
    int64_t p4 = SatMul(A.Max, B.Max);
    return ClampRangeToType(
        ValueRange::Interval(std::min({p1, p2, p3, p4}),
                             std::max({p1, p2, p3, p4})),
        TypeR);
  }
  if (Op == "/") {
    int64_t b_min = B.Min;
    int64_t b_max = B.Max;
    if (b_min == 0 && b_max == 0) return TypeR;
    if (b_min == 0) b_min = 1;
    if (b_max == 0) b_max = -1;
    if (b_min < 0 && b_max > 0) {
      int64_t max_mag = std::max(SatNeg(A.Min), A.Max);
      return ClampRangeToType(
          ValueRange::Interval(SatNeg(max_mag), max_mag), TypeR);
    }
    int64_t q1 = SafeDiv(A.Min, b_min);
    int64_t q2 = SafeDiv(A.Min, b_max);
    int64_t q3 = SafeDiv(A.Max, b_min);
    int64_t q4 = SafeDiv(A.Max, b_max);
    return ClampRangeToType(
        ValueRange::Interval(std::min({q1, q2, q3, q4}),
                             std::max({q1, q2, q3, q4})),
        TypeR);
  }
  if (Op == "%") {
    if (A.IsConstant() && B.IsConstant() && B.Min != 0 &&
        !(A.Min == std::numeric_limits<int64_t>::min() && B.Min == -1)) {
      return ClampRangeToType(ValueRange::Exact(A.Min % B.Min), TypeR);
    }
    int64_t max_b = std::max(SatNeg(B.Min), B.Max);
    if (max_b <= 0) return TypeR;
    int64_t bound = max_b - 1;
    if (A.Min >= 0) {
      if (B.Min > 0 && A.Max < B.Min) {
        return ClampRangeToType(A, TypeR);
      }
      return ClampRangeToType(
          ValueRange::Interval(0, std::min(A.Max, bound)), TypeR);
    }
    return ClampRangeToType(ValueRange::Interval(-bound, bound), TypeR);
  }
  if (Op == "<<") {
    if (B.Min >= 0 && B.Max <= 62) {
      int64_t s1 = SatShl(A.Min, B.Min);
      int64_t s2 = SatShl(A.Min, B.Max);
      int64_t s3 = SatShl(A.Max, B.Min);
      int64_t s4 = SatShl(A.Max, B.Max);
      return ClampRangeToType(
          ValueRange::Interval(std::min({s1, s2, s3, s4}),
                               std::max({s1, s2, s3, s4})),
          TypeR);
    }
    return TypeR;
  }
  if (Op == ">>") {
    if (B.Min >= 0 && B.Max <= 62) {
      int64_t s1 = A.Min >> B.Min;
      int64_t s2 = A.Min >> B.Max;
      int64_t s3 = A.Max >> B.Min;
      int64_t s4 = A.Max >> B.Max;
      return ClampRangeToType(
          ValueRange::Interval(std::min({s1, s2, s3, s4}),
                               std::max({s1, s2, s3, s4})),
          TypeR);
    }
    return TypeR;
  }
  if (Op == "|" || Op == "^") {
    if (A.IsConstant() && B.IsConstant()) {
      int64_t v = (Op == "|") ? (A.Min | B.Min) : (A.Min ^ B.Min);
      return ClampRangeToType(ValueRange::Exact(v), TypeR);
    }
    if (A.Min >= 0 && B.Min >= 0) {
      int64_t max_ab = std::max(A.Max, B.Max);
      if (max_ab == 0) return ClampRangeToType(ValueRange::Exact(0), TypeR);
      uint64_t u = static_cast<uint64_t>(max_ab);
      int bits = 64 - __builtin_clzll(u);
      int64_t hi = (bits >= 63) ? std::numeric_limits<int64_t>::max()
                                : static_cast<int64_t>((1ULL << bits) - 1);
      int64_t lo = (Op == "|") ? std::max(A.Min, B.Min) : 0;
      return ClampRangeToType(ValueRange::Interval(lo, hi), TypeR);
    }
    return TypeR;
  }
  return TypeR;
}

ValueRange CombineNodeRangeFromOperandRanges(
    const Node* N, const std::vector<ValueRange>& OperandRanges,
    bool HasIncompleteSources) {
  if (!N) return ValueRange::Unknown();
  ValueRange TypeR = GetIntrinsicRange(N);
  if (HasIncompleteSources) return TypeR;

  if (N->Kind == NodeKind::Literal || N->Type.rfind(" = ") != std::string::npos) {
    return TypeR;
  }

  // In-sandbox heap fields or direct sandbox heap reads can hold any bit
  // pattern representable by their storage type or bitfield width, regardless
  // of narrower constants observed at subset write sites. Trusted-space fields
  // are considered safe and derive their value range from incoming writes.
  if (GetIntrinsicHeapTaint(N) == HeapTaint::InSandbox) {
    return TypeR;
  }

  if (OperandRanges.empty()) {
    return TypeR;
  }

  if (N->Kind == NodeKind::Expression) {
    if (OperandRanges.size() == 1 && IsUnaryOpName(N->Name)) {
      return EvalUnaryOp(N->Name, OperandRanges[0], TypeR);
    }
    if (OperandRanges.size() >= 2 && (N->Name == "++" || N->Name == "--")) {
      return EvalUnaryOp(N->Name, OperandRanges.back(), TypeR);
    }
    if (OperandRanges.size() == 2 && IsBinaryOpName(N->Name)) {
      return EvalBinaryOp(N->Name, OperandRanges[0], OperandRanges[1], TypeR);
    }
    if (OperandRanges.size() == 1 && IsBinaryOpName(N->Name) &&
        N->Name != "-") {
      return EvalBinaryOp(N->Name, OperandRanges[0], OperandRanges[0], TypeR);
    }
    if (IsOperatorExprName(N->Name)) {
      return TypeR;
    }
    if (N->Name.find("::update()") != std::string::npos &&
        N->Type.rfind(" : ") != std::string::npos) {
      return ClampRangeToType(OperandRanges.back(), TypeR);
    }
  }

  // Union join across all incoming assignments / arguments / returns
  int64_t lo = std::numeric_limits<int64_t>::max();
  int64_t hi = std::numeric_limits<int64_t>::min();
  for (const auto& R : OperandRanges) {
    if (!R.HasValue) {
      return TypeR;
    }
    lo = std::min(lo, R.Min);
    hi = std::max(hi, R.Max);
  }
  if (lo <= hi) {
    return ClampRangeToType(ValueRange::Interval(lo, hi), TypeR);
  }
  return TypeR;
}

bool IsSelfIncDecSource(Database& Db, Node::Id VarId, Node::Id SrcId,
                        std::vector<Node::Id>* OutBoundIds = nullptr) {
  const Node* Src = Db.GetNode(SrcId);
  if (!Src || Src->Kind != NodeKind::Expression) return false;
  if (Src->Name != "++" && Src->Name != "--") return false;
  bool HasSelfEdge = false;
  for (const auto& SubE : Db.GetIncomingEdges(SrcId)) {
    if (SubE.From == VarId) {
      HasSelfEdge = true;
    } else if (OutBoundIds && SubE.From != 0) {
      OutBoundIds->push_back(SubE.From);
    }
  }
  if (!HasSelfEdge && OutBoundIds) {
    OutBoundIds->clear();
  }
  return HasSelfEdge;
}

void GetSelfIncDecFlags(Database& Db, const Node* N,
                        const SourceLocation& FlowLoc, bool& HasSelfInc,
                        bool& HasSelfDec, int64_t& IncBound, int64_t& DecBound,
                        std::vector<Node::Id>* OutIncBoundIds = nullptr,
                        std::vector<Node::Id>* OutDecBoundIds = nullptr) {
  HasSelfInc = false;
  HasSelfDec = false;
  if (!N) return;
  if (N->Kind != NodeKind::Variable && N->Kind != NodeKind::Parameter &&
      N->Kind != NodeKind::Field) {
    return;
  }
  const auto& InEdges = Db.GetIncomingEdges(N->ID);
  for (const auto& E : InEdges) {
    if (E.Kind != EdgeKind::Assignment) continue;
    if ((N->Kind == NodeKind::Variable || N->Kind == NodeKind::Parameter) &&
        FlowLoc.IsValid() && FlowLoc.File == N->Loc.File &&
        E.Loc.File == FlowLoc.File && E.Loc.Line > FlowLoc.Line) {
      continue;
    }
    const Node* Src = Db.GetNode(E.From);
    if (!Src || Src->Kind != NodeKind::Expression) continue;
    if (Src->Name != "++" && Src->Name != "--") continue;
    std::vector<Node::Id> BoundIds;
    if (!IsSelfIncDecSource(Db, N->ID, Src->ID, &BoundIds)) continue;
    if (Src->Name == "++") {
      HasSelfInc = true;
      int64_t B = 0;
      if (GetIntrinsicLoopBound(Src, B)) {
        IncBound = B;
      } else if (OutIncBoundIds) {
        OutIncBoundIds->insert(OutIncBoundIds->end(), BoundIds.begin(),
                               BoundIds.end());
      }
    }
    if (Src->Name == "--") {
      HasSelfDec = true;
      int64_t B = 0;
      if (GetIntrinsicLoopBound(Src, B)) {
        DecBound = B;
      } else if (OutDecBoundIds) {
        OutDecBoundIds->insert(OutDecBoundIds->end(), BoundIds.begin(),
                               BoundIds.end());
      }
    }
  }
}

bool IsPublicIncludeFile(const std::string& File) {
  if (File.empty()) return false;
  if (File.rfind("include/", 0) == 0) return true;
  if (File.rfind("third_party/", 0) == 0 ||
      File.find("/third_party/") != std::string::npos) {
    return false;
  }
  return File.find("/include/") != std::string::npos;
}

bool IsApiExposedNode(Database& Db, const Node* N) {
  if (!N) return false;
  if (N->Kind == NodeKind::Field) {
    return IsPublicIncludeFile(N->Loc.File) &&
           N->Type.rfind(" = ") == std::string::npos;
  }
  if (N->Kind != NodeKind::Parameter) return false;
  if (IsPublicIncludeFile(N->Loc.File)) return true;

  std::unordered_set<Node::Id> Visited;
  std::vector<Node::Id> Queue;
  Visited.insert(N->ID);
  Queue.push_back(N->ID);

  while (!Queue.empty() && Visited.size() <= 32) {
    Node::Id CurId = Queue.back();
    Queue.pop_back();

    for (const auto& E : Db.GetIncomingEdges(CurId)) {
      if (E.Kind == EdgeKind::Implicit && E.From != 0 &&
          Visited.find(E.From) == Visited.end()) {
        const Node* Src = Db.GetNode(E.From);
        if (Src && Src->Kind == NodeKind::Parameter) {
          if (IsPublicIncludeFile(Src->Loc.File)) return true;
          Visited.insert(E.From);
          Queue.push_back(E.From);
        }
      }
    }

    for (const auto& E : Db.GetOutgoingEdges(CurId)) {
      if (E.Kind == EdgeKind::Implicit && E.To != 0 &&
          Visited.find(E.To) == Visited.end()) {
        const Node* Dst = Db.GetNode(E.To);
        if (Dst && Dst->Kind == NodeKind::Parameter) {
          if (IsPublicIncludeFile(Dst->Loc.File)) return true;
          Visited.insert(E.To);
          Queue.push_back(E.To);
        }
      }
    }
  }
  return false;
}

HeapTaint GetEffectiveIntrinsicTaint(Database& Db, const Node* N,
                                     bool AllowApiEmbedderTaint = true) {
  HeapTaint T = GetIntrinsicHeapTaint(N);
  if (AllowApiEmbedderTaint && IsApiExposedNode(Db, N)) {
    T = CombineHeapTaint(T, HeapTaint::Embedder);
  }
  return T;
}

bool SubtreeHasNonCycleLeaf(const QueryResultNode* Node) {
  if (!Node) return false;
  if (Node->IsTruncated) return true;
  if (Node->IsCycle) return false;
  if (Node->Children.empty()) return true;
  for (const auto& C : Node->Children) {
    if (SubtreeHasNonCycleLeaf(C.get())) return true;
  }
  return false;
}

void ComputeNodeValueRange(Database& Db, QueryResultNode* Result,
                           bool ForceIntrinsicRange = false) {
  if (!Result || !Result->TargetNode) return;
  const Node* N = Result->TargetNode;
  bool Incomplete =
      Result->IsCycle || Result->IsTruncated || ForceIntrinsicRange;
  bool IsOpExpr =
      (N->Kind == NodeKind::Expression && IsOperatorExprName(N->Name));

  bool HasLiveChild = false;
  bool HasNonCycleChild = false;
  if (!IsOpExpr) {
    for (const auto& C : Result->Children) {
      if (!C) continue;
      if (!C->IsDead) {
        HasLiveChild = true;
      }
      if (SubtreeHasNonCycleLeaf(C.get())) {
        HasNonCycleChild = true;
      }
    }
  }

  std::vector<ValueRange> ChildRanges;
  std::vector<ValueRange> BaseChildRanges;
  std::vector<QueryResultNode*> SelfIncDecChildren;
  bool HasSelfInc = false;
  bool HasSelfDec = false;
  ChildRanges.reserve(Result->Children.size());
  HeapTaint Taint = GetEffectiveIntrinsicTaint(Db, N, ForceIntrinsicRange);
  for (const auto& C : Result->Children) {
    if (!C) continue;
    Taint = CombineHeapTaint(Taint, C->Taint);
    if (!IsOpExpr && HasLiveChild && C->IsDead) {
      continue;
    }
    bool IsSelfIncDec = false;
    if (!IsOpExpr && C->TargetNode &&
        C->TargetNode->Kind == NodeKind::Expression &&
        (C->TargetNode->Name == "++" || C->TargetNode->Name == "--")) {
      for (const auto& SubC : C->Children) {
        if (SubC && SubC->IsCycle && SubC->TargetNode &&
            SubC->TargetNode->ID == N->ID) {
          IsSelfIncDec = true;
          break;
        }
      }
      if (IsSelfIncDec) {
        if (C->TargetNode->Name == "++") HasSelfInc = true;
        if (C->TargetNode->Name == "--") HasSelfDec = true;
        SelfIncDecChildren.push_back(C.get());
      }
    }
    if (!IsOpExpr && !IsSelfIncDec && HasNonCycleChild &&
        !SubtreeHasNonCycleLeaf(C.get())) {
      continue;
    }
    ChildRanges.push_back(C->Range);
    if (!IsSelfIncDec) {
      BaseChildRanges.push_back(C->Range);
    }
  }
  if ((HasSelfInc || HasSelfDec) && !BaseChildRanges.empty()) {
    ValueRange TypeR = GetIntrinsicRange(N);
    int64_t TypeMin = TypeR.HasValue ? TypeR.Min : -2147483648LL;
    int64_t TypeMax = TypeR.HasValue ? TypeR.Max : 2147483647LL;
    for (QueryResultNode* SelfC : SelfIncDecChildren) {
      int64_t B = 0;
      if (GetIntrinsicLoopBound(SelfC->TargetNode, B)) {
        if (SelfC->TargetNode->Name == "++") TypeMax = B;
        if (SelfC->TargetNode->Name == "--") TypeMin = B;
      } else {
        for (const auto& SubC : SelfC->Children) {
          if (SubC && SubC->TargetNode && SubC->TargetNode->ID != N->ID &&
              SubC->Range.HasValue) {
            if (SelfC->TargetNode->Name == "++") {
              TypeMax = SubC->Range.Max;
            }
            if (SelfC->TargetNode->Name == "--") {
              TypeMin = SubC->Range.Min;
            }
            break;
          }
        }
      }
    }
    ValueRange BaseR =
        CombineNodeRangeFromOperandRanges(N, BaseChildRanges, Incomplete);
    if (BaseR.HasValue && GetIntrinsicHeapTaint(N) != HeapTaint::InSandbox) {
      int64_t lo = HasSelfDec ? std::min(BaseR.Min, TypeMin) : BaseR.Min;
      int64_t hi = HasSelfInc ? std::max(BaseR.Max, TypeMax) : BaseR.Max;
      Result->Range = ClampRangeToType(ValueRange::Interval(lo, hi), TypeR);
    } else {
      Result->Range = BaseR;
    }
    for (QueryResultNode* SelfC : SelfIncDecChildren) {
      for (const auto& SubC : SelfC->Children) {
        if (SubC && SubC->IsCycle && SubC->TargetNode &&
            SubC->TargetNode->ID == N->ID) {
          SubC->Range = Result->Range;
        }
      }
      ValueRange SelfTypeR = GetIntrinsicRange(SelfC->TargetNode);
      if (Result->Range.HasValue && SelfC->TargetNode->Name == "++") {
        SelfC->Range = ClampRangeToType(
            ValueRange::Interval(
                std::min(SatAdd(Result->Range.Min, 1), Result->Range.Max),
                Result->Range.Max),
            SelfTypeR);
      } else if (Result->Range.HasValue && SelfC->TargetNode->Name == "--") {
        SelfC->Range = ClampRangeToType(
            ValueRange::Interval(
                Result->Range.Min,
                std::max(SatSub(Result->Range.Max, 1), Result->Range.Min)),
            SelfTypeR);
      } else {
        SelfC->Range =
            EvalUnaryOp(SelfC->TargetNode->Name, Result->Range, SelfTypeR);
      }
    }
  } else {
    Result->Range =
        CombineNodeRangeFromOperandRanges(N, ChildRanges, Incomplete);
  }
  Result->Taint = Taint;
}

}  // namespace

llvm::json::Value QueryResultNode::ToJson(
    const PathTranslator& Translator) const {
  std::set<Node::Id> Serialized;
  return ToJson(Translator, Serialized);
}

llvm::json::Value QueryResultNode::ToJson(
    const PathTranslator& Translator, std::set<Node::Id>& Serialized) const {
  llvm::json::Object Obj;
  Node::Id NodeId = 0;
  if (TargetNode) {
    NodeId = TargetNode->ID;
    llvm::json::Object LocObj{
        {"file", Translator.ToAbsolute(TargetNode->Loc.File)},
        {"line", static_cast<int64_t>(TargetNode->Loc.Line)},
        {"column", static_cast<int64_t>(TargetNode->Loc.Column)},
    };
    llvm::json::Object NodeObj{
        {"id", std::to_string(TargetNode->ID)}, // Serialize as string to protect JS float precision!
        {"kind", NodeKindToString(TargetNode->Kind)},
        {"name", TargetNode->Name},
        {"type", StripHeapTaintSuffix(TargetNode->Type)},
        {"location", std::move(LocObj)},
    };
    Obj["node"] = std::move(NodeObj);
  } else {
    Obj["node"] = nullptr;
  }

  Obj["flow_kind"] = EdgeKindToString(FlowKind);
  Obj["is_cycle"] = IsCycle;
  Obj["is_cached"] = IsCached;
  if (Range.HasValue) {
    llvm::json::Object RangeObj{
        {"min", Range.Min},
        {"max", Range.Max},
        {"display", Range.ToDisplayString()},
    };
    Obj["value_range"] = std::move(RangeObj);
  }
  if (Taint != HeapTaint::None) {
    Obj["heap_taint"] = HeapTaintToString(Taint);
  }
  if (ContributedRange.HasValue) {
    llvm::json::Object ContribObj{
        {"min", ContributedRange.Min},
        {"max", ContributedRange.Max},
        {"display", ContributedRange.ToDisplayString()},
    };
    Obj["contributed_range"] = std::move(ContribObj);
  }
  if (PathTaint != HeapTaint::None) {
    Obj["path_taint"] = HeapTaintToString(PathTaint);
  }
  if (ClobberedByHeap) {
    Obj["clobbered_by_heap"] = true;
  }
  if (IsDead) {
    Obj["is_dead"] = true;
  }

  llvm::json::Object FlowLocObj{
      {"file", FlowLoc.File.empty() ? "" : Translator.ToAbsolute(FlowLoc.File)},
      {"line", static_cast<int64_t>(FlowLoc.Line)},
      {"column", static_cast<int64_t>(FlowLoc.Column)},
  };
  Obj["flow_location"] = std::move(FlowLocObj);

  llvm::json::Array ChildrenArr;

  // Check if this node has already been serialized with children in the tree,
  // or if we have reached the JSON serialization node budget (50000 unique expanded nodes).
  bool AlreadyExpanded =
      (NodeId != 0 && Serialized.find(NodeId) != Serialized.end());
  bool BudgetReached =
      (NodeId != 0 && !AlreadyExpanded && Serialized.size() >= 50000);
  if (!Children.empty() && (AlreadyExpanded || BudgetReached)) {
    Obj["is_cached"] = AlreadyExpanded;
    Obj["is_truncated"] = IsTruncated || BudgetReached;
    Obj["children"] = std::move(ChildrenArr);  // Return empty children vector
  } else {
    if (NodeId != 0 && !Children.empty()) {
      Serialized.insert(NodeId);
    }
    for (const auto& Child : Children) {
      if (!Child) continue;
      ChildrenArr.push_back(Child->ToJson(Translator, Serialized));
    }
    Obj["is_cached"] = false;
    Obj["is_truncated"] = IsTruncated;
    Obj["children"] = std::move(ChildrenArr);
  }

  return llvm::json::Value(std::move(Obj));
}

std::shared_ptr<QueryResultNode> QueryEngine::QueryIncoming(
    const std::string& File, unsigned Line, unsigned Col) {
  std::string RelFile = Translator.ToRelative(File);
  const Node* Target = Db.FindNodeByLocation(RelFile, Line, Col);
  if (!Target) {
    std::cerr << "[QueryEngine] Node not found at " << File << ":" << Line
              << ":" << Col << std::endl;
    return nullptr;
  }

  std::cerr << "[QueryEngine] QueryIncoming: Found target node: " << Target->ID
            << std::endl;
  std::unordered_map<Node::Id, int> VisitedDepths;
  std::unordered_map<Node::Id, std::shared_ptr<QueryResultNode>> Memo;
  int MinReachedDepth = std::numeric_limits<int>::max();
  SourceLocation InitialFlowLoc = (Line > Target->Loc.Line)
                                      ? SourceLocation{RelFile, Line, Col}
                                      : SourceLocation();
  return TraverseIncoming(Target, EdgeKind::Unknown, InitialFlowLoc,
                          VisitedDepths, 0, MinReachedDepth, Memo);
}

std::shared_ptr<QueryResultNode> QueryEngine::QueryOutgoing(
    const std::string& File, unsigned Line, unsigned Col) {
  std::string RelFile = Translator.ToRelative(File);
  const Node* Target = Db.FindNodeByLocation(RelFile, Line, Col);
  if (!Target) {
    std::cerr << "[QueryEngine] Node not found at " << File << ":" << Line
              << ":" << Col << std::endl;
    return nullptr;
  }

  // If Target is an out-of-line definition's _return node (connected via
  // CanonicalReturn -> DefReturn Implicit edge) with no direct outgoing edges,
  // redirect to the canonical _return node where CallExpr Return edges attach.
  if (Target->Name.find("_return") != std::string::npos &&
      Db.GetOutgoingEdges(Target->ID).empty()) {
    for (const auto& e : Db.GetIncomingEdges(Target->ID)) {
      if (e.Kind == EdgeKind::Implicit) {
        if (const Node* CanonicalRet = Db.GetNode(e.From)) {
          if (CanonicalRet->Name.find("_return") != std::string::npos) {
            Target = CanonicalRet;
            break;
          }
        }
      }
    }
  }

  std::cerr << "[QueryEngine] QueryOutgoing: Found target node: " << Target->ID
            << std::endl;
  std::unordered_map<Node::Id, int> VisitedDepths;
  std::unordered_map<Node::Id, std::shared_ptr<QueryResultNode>> Memo;
  int MinReachedDepth = std::numeric_limits<int>::max();
  auto Result = TraverseOutgoing(Target, EdgeKind::Unknown, SourceLocation(),
                                 VisitedDepths, 0, MinReachedDepth, Memo);
  std::unordered_set<const QueryResultNode*> VisitedTaint;
  auto PropagateOutgoingTaint = [&](auto& self, QueryResultNode* Node,
                                    HeapTaint Upstream) -> void {
    if (!Node) return;
    HeapTaint Own = Node->TargetNode
                        ? GetEffectiveIntrinsicTaint(Db, Node->TargetNode, true)
                        : HeapTaint::None;
    HeapTaint Combined =
        CombineHeapTaint(Upstream, CombineHeapTaint(Node->Taint, Own));
    bool FirstVisit = VisitedTaint.insert(Node).second;
    if (!FirstVisit && Node->Taint == Combined) {
      return;
    }
    Node->Taint = Combined;
    for (auto& C : Node->Children) {
      self(self, C.get(), Node->Taint);
    }
  };
  PropagateOutgoingTaint(PropagateOutgoingTaint, Result.get(), HeapTaint::None);
  return Result;
}

static bool IsValidAllocationExprLeaf(Database& Db, const Node* N) {
  if (!N || N->Kind != NodeKind::Expression || N->Name != "expr") {
    return false;
  }
  if (N->Type.find('*') == std::string::npos) {
    return false;
  }
  if (!Db.GetIncomingEdges(N->ID).empty()) {
    return false;
  }
  for (const auto& E : Db.GetOutgoingEdges(N->ID)) {
    if (E.Kind == EdgeKind::Initialization && E.Loc.Line == N->Loc.Line) {
      const Node* Dst = Db.GetNode(E.To);
      if (Dst && Dst->Kind == NodeKind::Variable) {
        return true;
      }
    }
  }
  return false;
}

static bool IsDeadExpressionArtifact(
    Database& Db, const std::shared_ptr<QueryResultNode>& N) {
  if (!N || !N->TargetNode || !N->Children.empty() || N->IsCycle ||
      N->IsTruncated) {
    return false;
  }
  if (N->TargetNode->Kind != NodeKind::Expression) return false;
  if (N->TargetNode->Name == "call()") return true;
  if (N->TargetNode->Name == "expr" &&
      !IsValidAllocationExprLeaf(Db, N->TargetNode)) {
    return true;
  }
  return false;
}

static bool IsDeadSyntheticNode(const std::shared_ptr<QueryResultNode>& N) {
  return N && N->TargetNode && N->TargetNode->Name.rfind("DEAD: ", 0) == 0;
}

static bool HasLiveCoLocatedParameter(Database& Db, const Node* N);

static void PruneDeadChildrenIfLiveExists(
    Database& Db, std::vector<std::shared_ptr<QueryResultNode>>& Vec) {
  if (Vec.empty()) return;

  // Always strip childless "call()" and non-allocation "expr" artifacts,
  // even if Vec.size() == 1.
  size_t WriteIdx = 0;
  for (size_t i = 0; i < Vec.size(); ++i) {
    if (Vec[i] && !IsDeadExpressionArtifact(Db, Vec[i])) {
      if (WriteIdx != i) Vec[WriteIdx] = std::move(Vec[i]);
      ++WriteIdx;
    }
  }
  Vec.resize(WriteIdx);

  if (Vec.size() <= 1) return;
  bool HasLiveChild = false;
  for (const auto& C : Vec) {
    if (C && !IsDeadSyntheticNode(C) && !C->IsDead) {
      HasLiveChild = true;
      break;
    }
  }
  if (!HasLiveChild) return;
  WriteIdx = 0;
  for (size_t i = 0; i < Vec.size(); ++i) {
    const auto& C = Vec[i];
    if (IsDeadSyntheticNode(C)) continue;
    if (C && C->IsDead && C->TargetNode &&
        C->TargetNode->Kind == NodeKind::Parameter && C->Children.empty() &&
        !C->IsCycle && !C->IsTruncated) {
      bool HasLiveSameParamSibling = false;
      for (size_t j = 0; j < Vec.size(); ++j) {
        if (i != j && Vec[j] && Vec[j]->TargetNode &&
            Vec[j]->TargetNode->Kind == NodeKind::Parameter &&
            Vec[j]->TargetNode->Name == C->TargetNode->Name &&
            (!Vec[j]->Children.empty() || !Vec[j]->IsDead)) {
          HasLiveSameParamSibling = true;
          break;
        }
      }
      if (HasLiveSameParamSibling ||
          HasLiveCoLocatedParameter(Db, C->TargetNode)) {
        continue;
      }
    }
    if (WriteIdx != i) Vec[WriteIdx] = std::move(Vec[i]);
    ++WriteIdx;
  }
  Vec.resize(WriteIdx);
}

static std::string GetNodeCoordKey(const Node* N, bool IncludeType = true) {
  if (!N || !N->Loc.IsValid() || N->Loc.Line == 0) return "";
  std::string Key = std::to_string(static_cast<int>(N->Kind)) + "|" + N->Name +
                    "|" + N->Loc.File + ":" + std::to_string(N->Loc.Line) +
                    ":" + std::to_string(N->Loc.Column);
  if (IncludeType) {
    Key += "|" + N->Type;
  }
  return Key;
}

static void UnwrapSameAsParentAndPruneDeadChildren(
    Database& Db, std::vector<std::shared_ptr<QueryResultNode>>& Vec,
    const Node* ParentNode = nullptr) {
  if (ParentNode && !Vec.empty()) {
    std::string ParentCoordKey = GetNodeCoordKey(ParentNode, false);
    for (int Pass = 0; Pass < 2; ++Pass) {
      bool Changed = false;
      std::vector<std::shared_ptr<QueryResultNode>> Expanded;
      Expanded.reserve(Vec.size());
      for (auto& C : Vec) {
        if (!C || !C->TargetNode) continue;
        bool IsSameAsParent =
            (C->TargetNode->ID == ParentNode->ID) ||
            (!ParentCoordKey.empty() &&
             GetNodeCoordKey(C->TargetNode, false) == ParentCoordKey);
        if (IsSameAsParent) {
          Changed = true;
          for (const auto& GrandChild : C->Children) {
            if (GrandChild) Expanded.push_back(GrandChild);
          }
        } else {
          Expanded.push_back(std::move(C));
        }
      }
      Vec = std::move(Expanded);
      if (!Changed) break;
    }
  }

  PruneDeadChildrenIfLiveExists(Db, Vec);
}

static void DeduplicateChildrenByNodeId(
    Database& Db, std::vector<std::shared_ptr<QueryResultNode>>& Vec,
    const Node* ParentNode = nullptr) {
  UnwrapSameAsParentAndPruneDeadChildren(Db, Vec, ParentNode);
  std::unordered_map<Node::Id, size_t> SeenIds;
  std::unordered_map<std::string, size_t> SeenCoords;
  size_t WriteIdx = 0;
  for (size_t i = 0; i < Vec.size(); ++i) {
    if (!Vec[i] || !Vec[i]->TargetNode) continue;
    const Node* T = Vec[i]->TargetNode;
    Node::Id Id = T->ID;
    std::string CoordKey = GetNodeCoordKey(T);

    auto id_it = (Id != 0) ? SeenIds.find(Id) : SeenIds.end();
    auto coord_it =
        (!CoordKey.empty()) ? SeenCoords.find(CoordKey) : SeenCoords.end();

    if (id_it != SeenIds.end()) {
      continue;
    }
    if (coord_it != SeenCoords.end()) {
      size_t ExistingIdx = coord_it->second;
      if (ExistingIdx < WriteIdx && Vec[ExistingIdx] &&
          !Vec[i]->Children.empty()) {
        if (Vec[ExistingIdx]->Children.empty()) {
          Vec[ExistingIdx] = std::move(Vec[i]);
        } else if (Vec[ExistingIdx]->Children.size() +
                       Vec[i]->Children.size() <=
                   32) {
          if (Vec[ExistingIdx].use_count() > 1) {
            Vec[ExistingIdx] =
                std::make_shared<QueryResultNode>(*Vec[ExistingIdx]);
          }
          for (auto& Child : Vec[i]->Children) {
            if (!Child || !Child->TargetNode) continue;
            std::string ChildCoord = GetNodeCoordKey(Child->TargetNode);
            bool Exists = false;
            for (const auto& Ex : Vec[ExistingIdx]->Children) {
              if (!Ex || !Ex->TargetNode) continue;
              if (Ex->TargetNode->ID == Child->TargetNode->ID ||
                  (!ChildCoord.empty() &&
                   GetNodeCoordKey(Ex->TargetNode) == ChildCoord)) {
                Exists = true;
                break;
              }
            }
            if (!Exists) {
              Vec[ExistingIdx]->Children.push_back(std::move(Child));
            }
          }
        }
      }
      if (Id != 0) SeenIds[Id] = ExistingIdx;
      continue;
    }

    if (Id != 0) SeenIds[Id] = WriteIdx;
    if (!CoordKey.empty()) SeenCoords[CoordKey] = WriteIdx;
    if (WriteIdx != i) Vec[WriteIdx] = std::move(Vec[i]);
    ++WriteIdx;
  }
  Vec.resize(WriteIdx);
}

static void CollectCandidateArgPassLocsFromEdges(
    Database& Db, const std::vector<Edge>& Edges,
    std::vector<SourceLocation>& OutLocs,
    std::unordered_set<Node::Id>& VisitedParams, int Depth = 0) {
  if (Depth > 2 || VisitedParams.size() > 128) return;
  for (const auto& e : Edges) {
    if (e.Kind == EdgeKind::ArgPass && e.Loc.IsValid()) {
      OutLocs.push_back(e.Loc);
    } else if (e.Kind == EdgeKind::Implicit && e.From != 0 &&
               VisitedParams.insert(e.From).second) {
      CollectCandidateArgPassLocsFromEdges(Db, Db.GetIncomingEdges(e.From),
                                           OutLocs, VisitedParams, Depth + 1);
      if (VisitedParams.size() > 128) return;
    }
  }
}

static bool MatchArgLocInCandidates(
    const std::vector<SourceLocation>& AllCandidates,
    const SourceLocation& CallSiteLoc, SourceLocation& OutBestLoc) {
  if (!CallSiteLoc.IsValid() || AllCandidates.empty()) return false;

  // Primary pass: in C++ source order, an argument inside a CallExpr begins at
  // or after the CallExpr's begin location. Pick the closest (Line, Column) >=
  // (CallSiteLoc.Line, CallSiteLoc.Column) within 8 lines so back-to-back or
  // same-line calls never bleed into one another.
  bool FoundPrimary = false;
  for (const auto& Loc : AllCandidates) {
    if (Loc.File != CallSiteLoc.File) continue;
    bool IsAtOrAfter =
        (Loc.Line > CallSiteLoc.Line && Loc.Line <= CallSiteLoc.Line + 8) ||
        (Loc.Line == CallSiteLoc.Line && Loc.Column >= CallSiteLoc.Column);
    if (IsAtOrAfter) {
      if (!FoundPrimary || Loc.Line < OutBestLoc.Line ||
          (Loc.Line == OutBestLoc.Line && Loc.Column < OutBestLoc.Column)) {
        OutBestLoc = Loc;
        FoundPrimary = true;
      }
    }
  }
  if (FoundPrimary) return true;

  // Fallback for macro expansions where spelling/expansion order may precede
  // CallSiteLoc on the same line or within 2 lines.
  bool FoundFallback = false;
  int BestDist = std::numeric_limits<int>::max();
  for (const auto& Loc : AllCandidates) {
    if (Loc.File != CallSiteLoc.File) continue;
    int LineDiff =
        std::abs(static_cast<int>(Loc.Line) - static_cast<int>(CallSiteLoc.Line));
    if (LineDiff <= 2) {
      int ColDiff = std::abs(static_cast<int>(Loc.Column) -
                             static_cast<int>(CallSiteLoc.Column));
      int Dist = LineDiff * 10000 + ColDiff;
      if (!FoundFallback || Dist < BestDist) {
        BestDist = Dist;
        OutBestLoc = Loc;
        FoundFallback = true;
      }
    }
  }
  return FoundFallback;
}

static bool NodeHasExactArgPassImpl(Database& Db, Node::Id ParamId,
                                    const SourceLocation& ExactArgLoc,
                                    std::unordered_set<Node::Id>& VisitedParams,
                                    int Depth) {
  if (Depth > 2 || VisitedParams.size() > 64 ||
      !VisitedParams.insert(ParamId).second) {
    return false;
  }
  for (const auto& e : Db.GetIncomingEdges(ParamId)) {
    if (e.Kind == EdgeKind::ArgPass && e.Loc.File == ExactArgLoc.File &&
        e.Loc.Line == ExactArgLoc.Line && e.Loc.Column == ExactArgLoc.Column) {
      return true;
    }
    if (e.Kind == EdgeKind::Implicit && e.From != 0 &&
        NodeHasExactArgPassImpl(Db, e.From, ExactArgLoc, VisitedParams,
                                Depth + 1)) {
      return true;
    }
  }
  return false;
}

static bool NodeHasExactArgPass(Database& Db, Node::Id CurrentParamId,
                                Node::Id SourceParamId,
                                const SourceLocation& ExactArgLoc) {
  std::unordered_set<Node::Id> VisitedParams;
  if (CurrentParamId != 0) VisitedParams.insert(CurrentParamId);
  return NodeHasExactArgPassImpl(Db, SourceParamId, ExactArgLoc, VisitedParams,
                                 0);
}

static int FindMatchingCallStackIndex(
    Database& Db, const Node* ParamNode, const std::vector<Edge>& InEdges,
    const std::vector<SourceLocation>& CallStack,
    SourceLocation& OutBestArgLoc) {
  if (!ParamNode || ParamNode->Kind != NodeKind::Parameter ||
      CallStack.empty()) {
    return -1;
  }
  std::vector<SourceLocation> AllCandidates;
  std::unordered_set<Node::Id> VisitedParams;
  VisitedParams.insert(ParamNode->ID);
  CollectCandidateArgPassLocsFromEdges(Db, InEdges, AllCandidates,
                                       VisitedParams);
  if (AllCandidates.empty()) return -1;

  for (int idx = static_cast<int>(CallStack.size()) - 1; idx >= 0; --idx) {
    if (MatchArgLocInCandidates(AllCandidates, CallStack[idx], OutBestArgLoc)) {
      return idx;
    }
  }
  return -1;
}

static uint64_t ComputeCallStackHash(
    const std::vector<SourceLocation>& CallStack) {
  if (CallStack.empty()) return 0;
  const auto& Loc = CallStack.back();
  return HashFnv1a64(Loc.File) ^
         (static_cast<uint64_t>(Loc.Line) * 0x9e3779b97f4a7c15ULL) ^
         (static_cast<uint64_t>(Loc.Column) * 0xbf58476d1ce4e5b9ULL) ^
         (static_cast<uint64_t>(CallStack.size()) * 0x94d049bb133111ebULL);
}

static bool IsBogusArtifactNode(Database& Db, const Node* N) {
  if (!N) return true;
  if (N->Name.empty() && N->Kind != NodeKind::Parameter) return true;
  if (N->Kind == NodeKind::Parameter && N->Name == "args" &&
      Db.GetIncomingEdges(N->ID).empty()) {
    return true;
  }
  return false;
}

static bool IsDeadParameterNode(Database& Db, const Node* N) {
  if (!N || N->Kind != NodeKind::Parameter) return false;
  if (N->Name.empty() || N->Name == "this" || N->Name == "args") return false;
  if (IsPublicIncludeFile(N->Loc.File)) return false;

  std::unordered_set<Node::Id> Visited;
  std::vector<Node::Id> Queue;
  Visited.insert(N->ID);
  Queue.push_back(N->ID);

  while (!Queue.empty() && Visited.size() <= 32) {
    Node::Id CurId = Queue.back();
    Queue.pop_back();

    for (const auto& E : Db.GetIncomingEdges(CurId)) {
      if (E.Kind == EdgeKind::ArgPass) {
        return false;
      }
      if (E.Kind == EdgeKind::Implicit && E.From != 0 &&
          Visited.find(E.From) == Visited.end()) {
        const Node* Src = Db.GetNode(E.From);
        if (Src && Src->Kind == NodeKind::Parameter) {
          if (IsPublicIncludeFile(Src->Loc.File)) return false;
          Visited.insert(E.From);
          Queue.push_back(E.From);
        }
      }
    }

    for (const auto& E : Db.GetOutgoingEdges(CurId)) {
      if (E.Kind == EdgeKind::Implicit && E.To != 0 &&
          Visited.find(E.To) == Visited.end()) {
        const Node* Dst = Db.GetNode(E.To);
        if (Dst && Dst->Kind == NodeKind::Parameter) {
          if (IsPublicIncludeFile(Dst->Loc.File)) return false;
          Visited.insert(E.To);
          Queue.push_back(E.To);
        }
      }
    }
  }
  return true;
}

static bool HasLiveCoLocatedParameter(Database& Db, const Node* N) {
  if (!N || N->Kind != NodeKind::Parameter || !N->Loc.IsValid() ||
      N->Loc.Line == 0) {
    return false;
  }
  for (const auto& Sib :
       Db.FindNodesAtLocation(N->Loc.File, N->Loc.Line, N->Loc.Column)) {
    if (Sib.ID != N->ID && Sib.Kind == NodeKind::Parameter &&
        Sib.Name == N->Name && !IsDeadParameterNode(Db, &Sib)) {
      return true;
    }
  }
  return false;
}

// Ignore edges from a raw backing integer field (e.g. PropertyDetails::value_)
// into a BitField sub-node of the same class/file (emitted by compiler-generated
// copy constructors or copy assignments `this->value_ = other.value_`).
static bool IsSelfBackingFieldToBitFieldEdge(const Node* Source,
                                             const Node* Target) {
  if (!Source || !Target) return false;
  if (Target->Kind == NodeKind::Field &&
      Target->Type.rfind(" : ") != std::string::npos &&
      Source->Kind == NodeKind::Field &&
      Source->Type.rfind(" : ") == std::string::npos &&
      Source->Loc.File == Target->Loc.File) {
    return true;
  }
  return false;
}

std::shared_ptr<QueryResultNode> QueryEngine::TraverseIncoming(
    const Node* N, EdgeKind FlowKind, const SourceLocation& FlowLoc,
    std::unordered_map<Node::Id, int>& VisitedDepths, int Depth,
    int& MinReachedDepth,
    std::unordered_map<Node::Id, std::shared_ptr<QueryResultNode>>& Memo,
    const std::vector<SourceLocation>& CallStack) {
  if (!N || (Depth > 0 && IsBogusArtifactNode(Db, N))) return nullptr;

  auto Result = std::make_shared<QueryResultNode>();
  Result->TargetNode = N;
  Result->FlowKind = FlowKind;
  Result->FlowLoc = FlowLoc;
  Result->IsDead = IsDeadParameterNode(Db, N);

  // Path-local cycle detection
  auto visit_it = VisitedDepths.find(N->ID);
  if (visit_it != VisitedDepths.end()) {
    Result->IsCycle = true;
    Result->Range = GetIntrinsicRange(N);
    Result->Taint = GetEffectiveIntrinsicTaint(Db, N, CallStack.empty());
    MinReachedDepth = std::min(MinReachedDepth, visit_it->second);
    return Result;  // Return dummy pruned boundary representation
  }

  bool NeedsSameFileLineCheck =
      (N->Kind == NodeKind::Variable || N->Kind == NodeKind::Parameter) &&
      FlowLoc.IsValid() && FlowLoc.File == N->Loc.File;
  uint64_t StackHash =
      (Memo.size() < 2000) ? ComputeCallStackHash(CallStack) : 0;
  if (!NeedsSameFileLineCheck) {
    Node::Id FastMemoKey = N->ID ^ StackHash;
    auto fast_it = Memo.find(FastMemoKey);
    if (fast_it != Memo.end()) {
      auto CachedResult = std::make_shared<QueryResultNode>();
      CachedResult->TargetNode = N;
      CachedResult->FlowKind = FlowKind;
      CachedResult->FlowLoc = FlowLoc;
      CachedResult->Children = fast_it->second->Children;
      CachedResult->Range = fast_it->second->Range;
      CachedResult->Taint = fast_it->second->Taint;
      CachedResult->IsDead = fast_it->second->IsDead;
      return CachedResult;
    }
  }

  const auto& InEdges = Db.GetIncomingEdges(N->ID);

  if (!InEdges.empty() &&
      (Depth >= 45 || VisitedDepths.size() >= 60 || Memo.size() >= 6000)) {
    Result->IsTruncated = true;
    Result->Range = GetIntrinsicRange(N);
    Result->Taint = GetEffectiveIntrinsicTaint(Db, N, CallStack.empty());
    return Result;
  }

  // Temporal pruning only filters incoming edges when N is a Variable or
  // Parameter with a post-declaration reassignment in the same file as FlowLoc.
  // Key Memo on (N->ID, EffectiveLine) only when such a reassignment exists so
  // same-file SSA calls do not bypass Memo.
  bool HasReassignment = false;
  if (NeedsSameFileLineCheck) {
    for (const auto& e : InEdges) {
      if ((e.Kind == EdgeKind::Assignment ||
           e.Kind == EdgeKind::Initialization) &&
          e.Loc.Line > N->Loc.Line) {
        HasReassignment = true;
        break;
      }
    }
  }
  unsigned EffectiveLine = HasReassignment ? FlowLoc.Line : 0;
  Node::Id MemoKey =
      N->ID ^ (static_cast<uint64_t>(EffectiveLine) * 0x9e3779b97f4a7c15ULL) ^
      StackHash;

  // Global memoization cache hit
  auto memo_it = Memo.find(MemoKey);
  if (memo_it != Memo.end()) {
    auto CachedResult = std::make_shared<QueryResultNode>();
    CachedResult->TargetNode = N;
    CachedResult->FlowKind = FlowKind;
    CachedResult->FlowLoc = FlowLoc;
    CachedResult->Children = memo_it->second->Children;
    CachedResult->Range = memo_it->second->Range;
    CachedResult->Taint = memo_it->second->Taint;
    CachedResult->IsDead = memo_it->second->IsDead;
    return CachedResult;
  }

  VisitedDepths[N->ID] = Depth;
  int SubtreeMinDepth = std::numeric_limits<int>::max();

  SourceLocation MatchedArgLoc;
  int MatchedCallIdx =
      FindMatchingCallStackIndex(Db, N, InEdges, CallStack, MatchedArgLoc);

  int NextDepth = IsElidableNode(N) ? Depth : (Depth + 1);
  size_t UnmatchedExpanded = 0;
  for (const auto& e : InEdges) {
    const Node* Source = Db.GetNode(e.From);
    if (!Source || IsSelfBackingFieldToBitFieldEdge(Source, N)) continue;

    // Temporal flow pruning: avoid tracing into assignments that occur strictly
    // after the current read/write location in the same file where the
    // variable/param is declared.
    if ((e.Kind == EdgeKind::Assignment ||
         e.Kind == EdgeKind::Initialization) &&
        (N->Kind == NodeKind::Variable || N->Kind == NodeKind::Parameter) &&
        FlowLoc.IsValid() && FlowLoc.File == N->Loc.File &&
        e.Loc.File == FlowLoc.File && e.Loc.Line > FlowLoc.Line) {
      continue;
    }

    if (Source->Kind != NodeKind::Literal) {
      if (Memo.size() >= 6000 && InEdges.size() > 2 &&
          !IsOperatorExprName(N->Name)) {
        Result->IsTruncated = true;
        break;
      }
      if (MatchedCallIdx < 0 && e.Kind != EdgeKind::Implicit) {
        if (InEdges.size() > 32 && e.Loc.File != N->Loc.File &&
            (!FlowLoc.IsValid() || e.Loc.File != FlowLoc.File) &&
            UnmatchedExpanded >= 24) {
          Result->IsTruncated = true;
          continue;
        }
        ++UnmatchedExpanded;
        if (UnmatchedExpanded > 48) {
          Result->IsTruncated = true;
          break;
        }
      }
    }

    const std::vector<SourceLocation>* NextStackPtr = &CallStack;
    std::vector<SourceLocation> ModifiedStack;

    if (MatchedCallIdx >= 0) {
      if (e.Kind == EdgeKind::ArgPass) {
        if (e.Loc.File != MatchedArgLoc.File ||
            e.Loc.Line != MatchedArgLoc.Line ||
            e.Loc.Column != MatchedArgLoc.Column) {
          continue;
        }
        ModifiedStack.assign(CallStack.begin(),
                             CallStack.begin() + MatchedCallIdx);
        NextStackPtr = &ModifiedStack;
      } else if (e.Kind == EdgeKind::Implicit) {
        if (!NodeHasExactArgPass(Db, N->ID, e.From, MatchedArgLoc)) {
          continue;
        }
      } else {
        continue;
      }
    } else if (e.Kind == EdgeKind::Return &&
               N->Name.find("_return") == std::string::npos &&
               Source->Name.find("_return") != std::string::npos) {
      SourceLocation CallSiteLoc = e.Loc.IsValid() ? e.Loc : N->Loc;
      if (CallSiteLoc.IsValid()) {
        if (CallStack.size() >= 32) {
          ModifiedStack.assign(CallStack.end() - 31, CallStack.end());
        } else {
          ModifiedStack = CallStack;
        }
        ModifiedStack.push_back(CallSiteLoc);
        NextStackPtr = &ModifiedStack;
      }
    }

    int ChildMinDepth = std::numeric_limits<int>::max();
    auto Child = TraverseIncoming(Source, e.Kind, e.Loc, VisitedDepths,
                                  NextDepth, ChildMinDepth, Memo,
                                  *NextStackPtr);
    if (Child) {
      bool ShouldElide = IsElidableNode(Source);
      if (ShouldElide && Source->Kind == NodeKind::Parameter &&
          !Source->Name.empty() && Source->Name != "args" &&
          Child->Children.empty() && !Child->IsCycle && !Child->IsTruncated &&
          N->Kind != NodeKind::Parameter) {
        ShouldElide = false;
      }
      if (ShouldElide) {
        for (const auto& GrandChild : Child->Children) {
          Result->Children.push_back(
              std::make_shared<QueryResultNode>(*GrandChild));
        }
        if (Child->IsTruncated) Result->IsTruncated = true;
      } else {
        Result->Children.push_back(std::move(Child));
      }
    }
    SubtreeMinDepth = std::min(SubtreeMinDepth, ChildMinDepth);
  }
  bool IsUnmatchedApiEntry =
      (MatchedCallIdx < 0 && IsApiExposedNode(Db, N));
  UnwrapSameAsParentAndPruneDeadChildren(Db, Result->Children, N);
  if (!Result->IsDead && !IsUnmatchedApiEntry && !Result->Children.empty()) {
    if (N->Kind == NodeKind::Expression && IsOperatorExprName(N->Name)) {
      for (const auto& C : Result->Children) {
        if (C && C->IsDead) {
          Result->IsDead = true;
          break;
        }
      }
    } else {
      bool AllDead = true;
      for (const auto& C : Result->Children) {
        if (C && !C->IsDead) {
          AllDead = false;
          break;
        }
      }
      if (AllDead) {
        Result->IsDead = true;
      }
    }
  }
  ComputeNodeValueRange(Db, Result.get(), IsUnmatchedApiEntry);
  DeduplicateChildrenByNodeId(Db, Result->Children, N);

  VisitedDepths.erase(N->ID);
  Memo[MemoKey] = Result;
  MinReachedDepth = std::min(MinReachedDepth, SubtreeMinDepth);
  return Result;
}

std::shared_ptr<QueryResultNode> QueryEngine::TraverseOutgoing(
    const Node* N, EdgeKind FlowKind, const SourceLocation& FlowLoc,
    std::unordered_map<Node::Id, int>& VisitedDepths, int Depth,
    int& MinReachedDepth,
    std::unordered_map<Node::Id, std::shared_ptr<QueryResultNode>>& Memo) {
  if (!N || (Depth > 0 && IsBogusArtifactNode(Db, N))) return nullptr;

  auto Result = std::make_shared<QueryResultNode>();
  Result->TargetNode = N;
  Result->FlowKind = FlowKind;
  Result->FlowLoc = FlowLoc;
  Result->Range = GetIntrinsicRange(N);
  Result->Taint = GetEffectiveIntrinsicTaint(Db, N, true);
  Result->IsDead = IsDeadParameterNode(Db, N);

  // Path-local cycle detection
  auto visit_it = VisitedDepths.find(N->ID);
  if (visit_it != VisitedDepths.end()) {
    Result->IsCycle = true;
    MinReachedDepth = std::min(MinReachedDepth, visit_it->second);
    return Result;  // Return dummy pruned boundary representation
  }

  // Global memoization cache hit
  auto memo_it = Memo.find(N->ID);
  if (memo_it != Memo.end()) {
    auto CachedResult = std::make_shared<QueryResultNode>();
    CachedResult->TargetNode = N;
    CachedResult->FlowKind = FlowKind;
    CachedResult->FlowLoc = FlowLoc;
    CachedResult->Children = memo_it->second->Children;
    CachedResult->Range = memo_it->second->Range;
    CachedResult->Taint = memo_it->second->Taint;
    CachedResult->IsDead = memo_it->second->IsDead;
    return CachedResult;
  }

  const auto& OutEdges = Db.GetOutgoingEdges(N->ID);
  if (Depth >= 45 || VisitedDepths.size() >= 60 || Memo.size() >= 6000) {
    Result->IsTruncated = !OutEdges.empty();
    return Result;
  }

  VisitedDepths[N->ID] = Depth;
  int SubtreeMinDepth = std::numeric_limits<int>::max();

  int NextDepth = IsElidableNode(N) ? Depth : (Depth + 1);
  size_t OutExpanded = 0;
  for (const auto& e : OutEdges) {
    if (Memo.size() >= 6000) {
      Result->IsTruncated = true;
      break;
    }
    if (++OutExpanded > 64) {
      Result->IsTruncated = true;
      break;
    }
    const Node* Sink = Db.GetNode(e.To);
    if (Sink) {
      int ChildMinDepth = std::numeric_limits<int>::max();
      auto Child = TraverseOutgoing(Sink, e.Kind, e.Loc, VisitedDepths,
                                    NextDepth, ChildMinDepth, Memo);
      if (Child) {
        if (IsElidableNode(Sink)) {
          for (const auto& GrandChild : Child->Children) {
            auto Cloned = std::make_shared<QueryResultNode>(*GrandChild);
            if (Cloned->FlowKind == EdgeKind::Implicit &&
                e.Kind != EdgeKind::Unknown) {
              Cloned->FlowKind = e.Kind;
              if (e.Loc.IsValid()) Cloned->FlowLoc = e.Loc;
            }
            Result->Children.push_back(std::move(Cloned));
          }
          if (Child->IsTruncated) Result->IsTruncated = true;
        } else {
          Result->Children.push_back(std::move(Child));
        }
      }
      SubtreeMinDepth = std::min(SubtreeMinDepth, ChildMinDepth);
    }
  }
  DeduplicateChildrenByNodeId(Db, Result->Children, N);

  VisitedDepths.erase(N->ID);
  Memo[N->ID] = Result;
  MinReachedDepth = std::min(MinReachedDepth, SubtreeMinDepth);
  return Result;
}

bool QueryEngine::IsElidableNode(const Node* N) {
  if (!N) return false;

  if (N->Name.find("_return") != std::string::npos) {
    return true;
  }

  if (N->Kind == NodeKind::Parameter) {
    if (N->Name.empty() || N->Name == "args" ||
        Db.HasOutgoingImplicitEdge(N->ID)) {
      return true;
    }
  }

  return false;
}

std::shared_ptr<QueryResultNode> QueryEngine::QueryFromLeaves(
    const std::string& File, unsigned Line, unsigned Col) {
  std::string RelFile = Translator.ToRelative(File);
  const Node* Target = Db.FindNodeByLocation(RelFile, Line, Col);
  if (!Target) return nullptr;

  std::cerr << "[QueryEngine] QueryFromLeaves: Found target node: "
            << Target->ID << std::endl;

  // Single linear backward traversal computes both the LeafNodes set AND
  // the exact BackwardReachable / ForwardAdj / BackwardSources sets.
  std::unordered_set<Node::Id> BackwardReachable;
  std::unordered_map<Node::Id, std::vector<Edge>> ForwardAdj;
  std::unordered_map<Node::Id, std::vector<Node::Id>> BackwardSources;
  std::unordered_map<Node::Id, bool> MemoizedValidUpstream;
  std::unordered_map<Node::Id, int> NodeVisitCount;
  std::unordered_map<Node::Id, int> BackwardVisitedDepths;
  int RootMinReachedDepth = std::numeric_limits<int>::max();
  std::set<Node::Id> LeafNodes;
  SourceLocation InitialFlowLoc = (Line > Target->Loc.Line)
                                      ? SourceLocation{RelFile, Line, Col}
                                      : SourceLocation();
  FindLeafNodes(Target, InitialFlowLoc, BackwardReachable, ForwardAdj,
                BackwardSources, MemoizedValidUpstream, NodeVisitCount,
                BackwardVisitedDepths, 0, RootMinReachedDepth, LeafNodes);

  std::cerr << "[QueryEngine] QueryFromLeaves: Found " << LeafNodes.size()
            << " allocation/source leaves across " << BackwardReachable.size()
            << " backward-reachable nodes." << std::endl;

  // Compute ValueRange, HeapTaint, and Dead-Parameter status bottom-up from
  // leaves to Target over BackwardSources
  std::unordered_map<Node::Id, ValueRange> NodeRanges;
  std::unordered_map<Node::Id, HeapTaint> NodeTaints;
  std::unordered_map<Node::Id, bool> NodeDead;
  std::unordered_set<Node::Id> ActiveRangeStack;
  auto ComputeSubgraphRange = [&](auto& self, Node::Id Id) -> ValueRange {
    auto it = NodeRanges.find(Id);
    if (it != NodeRanges.end()) return it->second;
    const Node* N = Db.GetNode(Id);
    if (!N) return ValueRange::Unknown();
    bool IsApiLeaf =
        (LeafNodes.find(Id) != LeafNodes.end() && IsApiExposedNode(Db, N));
    ValueRange TypeR = GetIntrinsicRange(N);
    HeapTaint Taint = GetEffectiveIntrinsicTaint(Db, N, IsApiLeaf);
    bool Dead = IsDeadParameterNode(Db, N);
    if (ActiveRangeStack.find(Id) != ActiveRangeStack.end()) {
      return TypeR;
    }
    if (N->Kind == NodeKind::Literal ||
        N->Type.rfind(" = ") != std::string::npos) {
      NodeRanges[Id] = TypeR;
      NodeTaints[Id] = Taint;
      NodeDead[Id] = Dead;
      return TypeR;
    }
    ActiveRangeStack.insert(Id);
    bool IsOpExpr =
        (N->Kind == NodeKind::Expression && IsOperatorExprName(N->Name));
    std::vector<ValueRange> OpRanges;
    std::vector<ValueRange> LiveOpRanges;
    std::vector<ValueRange> BaseOpRanges;
    std::vector<ValueRange> BaseLiveOpRanges;
    std::vector<Node::Id> SelfIncDecSrcIds;
    bool AnySrcDead = false;
    bool AllSrcDead = true;
    bool HasSrcs = false;
    auto src_it = BackwardSources.find(Id);
    if (src_it != BackwardSources.end() && !src_it->second.empty()) {
      HasSrcs = true;
      for (Node::Id SrcId : src_it->second) {
        bool IsSelfIncDec = !IsOpExpr && IsSelfIncDecSource(Db, Id, SrcId);
        if (IsSelfIncDec) {
          SelfIncDecSrcIds.push_back(SrcId);
        }
        ValueRange SR = self(self, SrcId);
        OpRanges.push_back(SR);
        if (!IsSelfIncDec) {
          BaseOpRanges.push_back(SR);
        }
        Taint = CombineHeapTaint(Taint, NodeTaints[SrcId]);
        if (NodeDead[SrcId]) {
          AnySrcDead = true;
        } else {
          AllSrcDead = false;
          LiveOpRanges.push_back(SR);
          if (!IsSelfIncDec) {
            BaseLiveOpRanges.push_back(SR);
          }
        }
      }
    }
    bool HasSelfInc = false;
    bool HasSelfDec = false;
    int64_t TypeMin = TypeR.HasValue ? TypeR.Min : -2147483648LL;
    int64_t TypeMax = TypeR.HasValue ? TypeR.Max : 2147483647LL;
    std::vector<Node::Id> IncBoundIds;
    std::vector<Node::Id> DecBoundIds;
    GetSelfIncDecFlags(
        Db, N, (Id == Target->ID) ? InitialFlowLoc : SourceLocation(),
        HasSelfInc, HasSelfDec, TypeMax, TypeMin, &IncBoundIds, &DecBoundIds);
    for (Node::Id BoundId : IncBoundIds) {
      ValueRange BR = self(self, BoundId);
      if (BR.HasValue) {
        TypeMax = BR.Max;
      }
    }
    for (Node::Id BoundId : DecBoundIds) {
      ValueRange BR = self(self, BoundId);
      if (BR.HasValue) {
        TypeMin = BR.Min;
      }
    }
    ActiveRangeStack.erase(Id);
    if (!Dead && !IsApiLeaf && HasSrcs) {
      if (IsOpExpr) {
        Dead = AnySrcDead;
      } else {
        Dead = AllSrcDead;
      }
    }
    const std::vector<ValueRange>& ChosenOpRanges =
        (!BaseOpRanges.empty()) ? BaseOpRanges : OpRanges;
    const std::vector<ValueRange>& ChosenLiveOpRanges =
        (!BaseLiveOpRanges.empty()) ? BaseLiveOpRanges : LiveOpRanges;
    const std::vector<ValueRange>& EffectiveOpRanges =
        (!IsOpExpr && !AllSrcDead && !ChosenLiveOpRanges.empty())
            ? ChosenLiveOpRanges
            : ChosenOpRanges;
    ValueRange R = CombineNodeRangeFromOperandRanges(
        N, EffectiveOpRanges, IsApiLeaf);
    if (R.HasValue && GetIntrinsicHeapTaint(N) != HeapTaint::InSandbox) {
      if (HasSelfInc || HasSelfDec) {
        int64_t lo = HasSelfDec ? std::min(R.Min, TypeMin) : R.Min;
        int64_t hi = HasSelfInc ? std::max(R.Max, TypeMax) : R.Max;
        R = ClampRangeToType(ValueRange::Interval(lo, hi), TypeR);
      }
    }
    NodeRanges[Id] = R;
    for (Node::Id SelfSrcId : SelfIncDecSrcIds) {
      const Node* SelfSrc = Db.GetNode(SelfSrcId);
      if (!SelfSrc || !R.HasValue) continue;
      ValueRange SelfTypeR = GetIntrinsicRange(SelfSrc);
      if (SelfSrc->Name == "++") {
        NodeRanges[SelfSrcId] = ClampRangeToType(
            ValueRange::Interval(std::min(SatAdd(R.Min, 1), R.Max), R.Max),
            SelfTypeR);
      } else if (SelfSrc->Name == "--") {
        NodeRanges[SelfSrcId] = ClampRangeToType(
            ValueRange::Interval(R.Min, std::max(SatSub(R.Max, 1), R.Min)),
            SelfTypeR);
      }
    }
    NodeTaints[Id] = Taint;
    NodeDead[Id] = Dead;
    return R;
  };
  for (Node::Id Id : BackwardReachable) {
    ComputeSubgraphRange(ComputeSubgraphRange, Id);
  }

  auto Result = std::make_shared<QueryResultNode>();
  auto VirtualTarget = std::make_shared<Node>();
  VirtualTarget->ID = 0;
  VirtualTarget->Kind = NodeKind::Variable;
  VirtualTarget->Name = "[Flipped] Flow from Leaves to: " + Target->Name;
  VirtualTarget->Type = Target->Type;
  VirtualTarget->Loc = Target->Loc;

  Result->OwnedTargetNode = VirtualTarget;
  Result->TargetNode = VirtualTarget.get();
  Result->FlowKind = EdgeKind::Unknown;
  Result->FlowLoc = SourceLocation();
  Result->Range = ComputeSubgraphRange(ComputeSubgraphRange, Target->ID);
  Result->Taint = NodeTaints[Target->ID];
  Result->IsDead = NodeDead[Target->ID];

  std::unordered_map<Node::Id, std::shared_ptr<QueryResultNode>> Memo;
  for (Node::Id LeafId : LeafNodes) {
    const Node* LeafNode = Db.GetNode(LeafId);
    if (LeafNode) {
      std::unordered_map<Node::Id, int> ForwardVisitedDepths;
      int MinReachedDepth = std::numeric_limits<int>::max();
      TraverseOutgoingPruned(LeafNode, Target, EdgeKind::Unknown,
                             SourceLocation(), ForwardVisitedDepths, 0,
                             MinReachedDepth, BackwardReachable, ForwardAdj,
                             NodeRanges, NodeTaints, NodeDead, LeafNodes, Memo,
                             Result->Children);
    }
  }
  DeduplicateChildrenByNodeId(Db, Result->Children);

  // Compute each leaf's individual path taint, contributed value range at Target,
  // and whether all of its forward paths to Target are clobbered by an
  // intermediate heap-tainted read/field or dead parameter.
  for (const auto& LeafChild : Result->Children) {
    if (!LeafChild || !LeafChild->TargetNode) continue;
    const Node* LeafNode = LeafChild->TargetNode;
    Node::Id LeafId = LeafNode->ID;

    std::unordered_set<Node::Id> ReachableFromLeaf;
    std::vector<Node::Id> Queue;
    ReachableFromLeaf.insert(LeafId);
    Queue.push_back(LeafId);
    HeapTaint PathTaint = GetEffectiveIntrinsicTaint(Db, LeafNode, true);
    while (!Queue.empty()) {
      Node::Id CurrId = Queue.back();
      Queue.pop_back();
      auto adj_it = ForwardAdj.find(CurrId);
      if (adj_it == ForwardAdj.end()) continue;
      for (const auto& E : adj_it->second) {
        if (BackwardReachable.find(E.To) == BackwardReachable.end()) continue;
        if (ReachableFromLeaf.insert(E.To).second) {
          if (E.To != Target->ID) {
            const Node* NextNode = Db.GetNode(E.To);
            if (NextNode) {
              PathTaint =
                  CombineHeapTaint(PathTaint, GetIntrinsicHeapTaint(NextNode));
            }
          }
          Queue.push_back(E.To);
        }
      }
    }

    bool ReachesUnclobbered = (LeafId == Target->ID);
    if (!ReachesUnclobbered) {
      std::unordered_set<Node::Id> UnclobberedVisited;
      std::vector<Node::Id> UQueue;
      UnclobberedVisited.insert(LeafId);
      UQueue.push_back(LeafId);
      while (!UQueue.empty()) {
        Node::Id CurrId = UQueue.back();
        UQueue.pop_back();
        if (CurrId == Target->ID) {
          ReachesUnclobbered = true;
          break;
        }
        auto adj_it = ForwardAdj.find(CurrId);
        if (adj_it == ForwardAdj.end()) continue;
        for (const auto& E : adj_it->second) {
          if (ReachableFromLeaf.find(E.To) == ReachableFromLeaf.end()) continue;
          if (E.To != Target->ID) {
            const Node* NextNode = Db.GetNode(E.To);
            if (NextNode &&
                GetIntrinsicHeapTaint(NextNode) == HeapTaint::InSandbox) {
              continue;
            }
          }
          if (UnclobberedVisited.insert(E.To).second) {
            UQueue.push_back(E.To);
          }
        }
      }
    }

    bool ReachesLive = !NodeDead[LeafId] && (LeafId == Target->ID);
    if (!NodeDead[LeafId] && !ReachesLive) {
      std::unordered_set<Node::Id> LiveVisited;
      std::vector<Node::Id> LQueue;
      LiveVisited.insert(LeafId);
      LQueue.push_back(LeafId);
      while (!LQueue.empty()) {
        Node::Id CurrId = LQueue.back();
        LQueue.pop_back();
        if (CurrId == Target->ID) {
          ReachesLive = true;
          break;
        }
        auto adj_it = ForwardAdj.find(CurrId);
        if (adj_it == ForwardAdj.end()) continue;
        for (const auto& E : adj_it->second) {
          if (ReachableFromLeaf.find(E.To) == ReachableFromLeaf.end()) continue;
          if (E.To != Target->ID && NodeDead[E.To]) {
            continue;
          }
          if (LiveVisited.insert(E.To).second) {
            LQueue.push_back(E.To);
          }
        }
      }
    }

    LeafChild->PathTaint = PathTaint;
    LeafChild->ClobberedByHeap = !ReachesUnclobbered;
    if (!ReachesLive) {
      LeafChild->IsDead = true;
    }

    std::unordered_map<Node::Id, ValueRange> LeafRanges;
    std::unordered_set<Node::Id> ActiveLeafStack;
    auto ComputeLeafNodeRange = [&](auto& self, Node::Id Id) -> ValueRange {
      auto it = LeafRanges.find(Id);
      if (it != LeafRanges.end()) return it->second;
      const Node* N = Db.GetNode(Id);
      if (!N) return ValueRange::Unknown();
      ValueRange TypeR = GetIntrinsicRange(N);
      if (Id == LeafId || N->Kind == NodeKind::Literal ||
          N->Type.rfind(" = ") != std::string::npos) {
        LeafRanges[Id] = TypeR;
        return TypeR;
      }
      if (ActiveLeafStack.find(Id) != ActiveLeafStack.end()) {
        return TypeR;
      }
      ActiveLeafStack.insert(Id);

      std::vector<Node::Id> UnwrappedSrcs;
      auto src_it = BackwardSources.find(Id);
      if (src_it != BackwardSources.end()) {
        UnwrappedSrcs = src_it->second;
      }

      ValueRange R = TypeR;
      bool IsBinOp =
          (N->Kind == NodeKind::Expression && IsBinaryOpName(N->Name));
      bool IsUnOp = (N->Kind == NodeKind::Expression && IsUnaryOpName(N->Name));
      if (IsUnOp && !UnwrappedSrcs.empty()) {
        for (Node::Id SrcId : UnwrappedSrcs) {
          if (ReachableFromLeaf.find(SrcId) != ReachableFromLeaf.end()) {
            R = EvalUnaryOp(N->Name, self(self, SrcId), TypeR);
            break;
          }
        }
      } else if (IsBinOp && !UnwrappedSrcs.empty()) {
        if (UnwrappedSrcs.size() == 1 && N->Name != "-") {
          Node::Id S0 = UnwrappedSrcs[0];
          ValueRange R0 =
              (ReachableFromLeaf.find(S0) != ReachableFromLeaf.end())
                  ? self(self, S0)
                  : NodeRanges[S0];
          R = EvalBinaryOp(N->Name, R0, R0, TypeR);
        } else if (UnwrappedSrcs.size() >= 2) {
          Node::Id S0 = UnwrappedSrcs[0];
          Node::Id S1 = UnwrappedSrcs[1];
          ValueRange R0 =
              (ReachableFromLeaf.find(S0) != ReachableFromLeaf.end())
                  ? self(self, S0)
                  : NodeRanges[S0];
          ValueRange R1 =
              (ReachableFromLeaf.find(S1) != ReachableFromLeaf.end())
                  ? self(self, S1)
                  : NodeRanges[S1];
          R = EvalBinaryOp(N->Name, R0, R1, TypeR);
        }
      } else if (N->Kind == NodeKind::Expression &&
                 IsOperatorExprName(N->Name)) {
        R = TypeR;
      } else {
        int64_t lo = std::numeric_limits<int64_t>::max();
        int64_t hi = std::numeric_limits<int64_t>::min();
        bool AnyKnown = false;
        bool AnyUnknown = false;
        bool HasNonSelfSource = false;
        for (Node::Id SrcId : UnwrappedSrcs) {
          if (ReachableFromLeaf.find(SrcId) == ReachableFromLeaf.end()) {
            continue;
          }
          if (!IsSelfIncDecSource(Db, Id, SrcId)) {
            HasNonSelfSource = true;
            break;
          }
        }
        for (Node::Id SrcId : UnwrappedSrcs) {
          if (ReachableFromLeaf.find(SrcId) == ReachableFromLeaf.end()) {
            continue;
          }
          if (HasNonSelfSource && IsSelfIncDecSource(Db, Id, SrcId)) {
            continue;
          }
          ValueRange SR = self(self, SrcId);
          if (!SR.HasValue) {
            AnyUnknown = true;
          } else {
            AnyKnown = true;
            lo = std::min(lo, SR.Min);
            hi = std::max(hi, SR.Max);
          }
        }
        if (AnyKnown && !AnyUnknown && lo <= hi) {
          R = ClampRangeToType(ValueRange::Interval(lo, hi), TypeR);
        } else {
          R = TypeR;
        }
      }
      if (R.HasValue && GetIntrinsicHeapTaint(N) != HeapTaint::InSandbox) {
        bool HasSelfInc = false;
        bool HasSelfDec = false;
        int64_t TypeMin = TypeR.HasValue ? TypeR.Min : -2147483648LL;
        int64_t TypeMax = TypeR.HasValue ? TypeR.Max : 2147483647LL;
        std::vector<Node::Id> IncBoundIds;
        std::vector<Node::Id> DecBoundIds;
        GetSelfIncDecFlags(
            Db, N, (Id == Target->ID) ? InitialFlowLoc : SourceLocation(),
            HasSelfInc, HasSelfDec, TypeMax, TypeMin, &IncBoundIds,
            &DecBoundIds);
        for (Node::Id BoundId : IncBoundIds) {
          auto bit = NodeRanges.find(BoundId);
          if (bit != NodeRanges.end() && bit->second.HasValue) {
            TypeMax = bit->second.Max;
          }
        }
        for (Node::Id BoundId : DecBoundIds) {
          auto bit = NodeRanges.find(BoundId);
          if (bit != NodeRanges.end() && bit->second.HasValue) {
            TypeMin = bit->second.Min;
          }
        }
        if (HasSelfInc || HasSelfDec) {
          int64_t lo = HasSelfDec ? std::min(R.Min, TypeMin) : R.Min;
          int64_t hi = HasSelfInc ? std::max(R.Max, TypeMax) : R.Max;
          R = ClampRangeToType(ValueRange::Interval(lo, hi), TypeR);
        }
      }

      ActiveLeafStack.erase(Id);
      LeafRanges[Id] = R;
      return R;
    };
    LeafChild->ContributedRange =
        ComputeLeafNodeRange(ComputeLeafNodeRange, Target->ID);
  }

  return Result;
}

bool QueryEngine::FindLeafNodes(
    const Node* Current, const SourceLocation& FlowLoc,
    std::unordered_set<Node::Id>& BackwardReachable,
    std::unordered_map<Node::Id, std::vector<Edge>>& ForwardAdj,
    std::unordered_map<Node::Id, std::vector<Node::Id>>& BackwardSources,
    std::unordered_map<Node::Id, bool>& MemoizedValidUpstream,
    std::unordered_map<Node::Id, int>& NodeVisitCount,
    std::unordered_map<Node::Id, int>& VisitedDepths, int Depth,
    int& MinReachedDepth, std::set<Node::Id>& LeafNodes,
    const std::vector<SourceLocation>& CallStack) {
  if (!Current) return false;

  // Every node visited during backward search from Target can reach Target.
  BackwardReachable.insert(Current->ID);

  // Active recursion-stack cycle detection: a back-edge to an active ancestor
  // is not an independent upstream source.
  auto visit_it = VisitedDepths.find(Current->ID);
  if (visit_it != VisitedDepths.end()) {
    MinReachedDepth = std::min(MinReachedDepth, visit_it->second);
    return false;
  }

  bool NeedsSameFileLineCheck =
      (Current->Kind == NodeKind::Variable ||
       Current->Kind == NodeKind::Parameter) &&
      FlowLoc.IsValid() && FlowLoc.File == Current->Loc.File;
  uint64_t StackHash = (MemoizedValidUpstream.size() < 2000)
                           ? ComputeCallStackHash(CallStack)
                           : 0;
  if (!NeedsSameFileLineCheck) {
    Node::Id FastMemoKey = Current->ID ^ StackHash;
    auto fast_it = MemoizedValidUpstream.find(FastMemoKey);
    if (fast_it != MemoizedValidUpstream.end()) {
      return fast_it->second;
    }
  }

  auto visit_cnt_it = NodeVisitCount.find(Current->ID);
  if (visit_cnt_it != NodeVisitCount.end() && visit_cnt_it->second >= 2) {
    auto base_it = MemoizedValidUpstream.find(Current->ID);
    if (base_it != MemoizedValidUpstream.end()) {
      return base_it->second;
    }
    return false;
  }

  const auto& InEdges = Db.GetIncomingEdges(Current->ID);

  if (!InEdges.empty() &&
      (Depth >= 45 || VisitedDepths.size() >= 60 ||
       MemoizedValidUpstream.size() >= 6000 ||
       NodeVisitCount.size() >= 6000)) {
    if (IsElidableNode(Current)) {
      MemoizedValidUpstream[Current->ID] = false;
      return false;
    }
    LeafNodes.insert(Current->ID);
    MemoizedValidUpstream[Current->ID] = true;
    return true;
  }

  bool HasReassignment = false;
  if (NeedsSameFileLineCheck) {
    for (const auto& e : InEdges) {
      if ((e.Kind == EdgeKind::Assignment ||
           e.Kind == EdgeKind::Initialization) &&
          e.Loc.Line > Current->Loc.Line) {
        HasReassignment = true;
        break;
      }
    }
  }
  unsigned EffectiveLine = HasReassignment ? FlowLoc.Line : 0;
  Node::Id MemoKey =
      Current->ID ^
      (static_cast<uint64_t>(EffectiveLine) * 0x9e3779b97f4a7c15ULL) ^
      StackHash;

  auto memo_it = MemoizedValidUpstream.find(MemoKey);
  if (memo_it != MemoizedValidUpstream.end()) {
    return memo_it->second;
  }

  // Cap total visits per Node::Id across different CallStack contexts to
  // prevent exponential SCC cycle re-traversal in deep graphs.
  int VisitIdx = ++NodeVisitCount[Current->ID];
  if (VisitIdx > 2) {
    auto base_it = MemoizedValidUpstream.find(Current->ID);
    if (base_it != MemoizedValidUpstream.end()) {
      return base_it->second;
    }
    return false;
  }

  auto AddForwardEdge = [&](Node::Id FromId, Node::Id ToId, EdgeKind Kind,
                            const SourceLocation& Loc) {
    auto& Vec = ForwardAdj[FromId];
    for (const auto& Existing : Vec) {
      if (Existing.To == ToId && Existing.Kind == Kind &&
          Existing.Loc.Line == Loc.Line && Existing.Loc.Column == Loc.Column) {
        return;
      }
    }
    Vec.push_back(Edge{FromId, ToId, Kind, Loc});

    auto& SrcVec = BackwardSources[ToId];
    const Node* ToNode = Db.GetNode(ToId);
    bool AllowDuplicateOperand =
        (ToNode && ToNode->Kind == NodeKind::Expression &&
         IsOperatorExprName(ToNode->Name) && VisitIdx == 1);
    if (AllowDuplicateOperand ||
        std::find(SrcVec.begin(), SrcVec.end(), FromId) == SrcVec.end()) {
      SrcVec.push_back(FromId);
    }
  };

  auto IsDeadExprNode = [&](const Node* N) {
    if (IsBogusArtifactNode(Db, N)) return true;
    if (N->Kind != NodeKind::Expression) return false;
    if (N->Name == "call()") return true;
    if (N->Name == "expr" && !IsValidAllocationExprLeaf(Db, N)) {
      return true;
    }
    return false;
  };

  auto IsDeadExprSource = [&](const Node* Source) {
    if (IsBogusArtifactNode(Db, Source)) return true;
    if (!IsDeadExprNode(Source)) return false;
    return Db.GetIncomingEdges(Source->ID).empty();
  };

  SourceLocation MatchedArgLoc;
  int MatchedCallIdx = FindMatchingCallStackIndex(Db, Current, InEdges,
                                                  CallStack, MatchedArgLoc);

  size_t UnmatchedExpanded = 0;
  auto ShouldTraverseEdge = [&](const Edge& e, const Node* Source,
                                std::vector<SourceLocation>& ModifiedStack,
                                const std::vector<SourceLocation>*& NextStackPtr) {
    if (IsSelfBackingFieldToBitFieldEdge(Source, Current)) return false;
    if ((e.Kind == EdgeKind::Assignment ||
         e.Kind == EdgeKind::Initialization) &&
        (Current->Kind == NodeKind::Variable ||
         Current->Kind == NodeKind::Parameter) &&
        FlowLoc.IsValid() && FlowLoc.File == Current->Loc.File &&
        e.Loc.File == FlowLoc.File && e.Loc.Line > FlowLoc.Line) {
      return false;
    }
    if (Source->Kind != NodeKind::Literal) {
      if ((MemoizedValidUpstream.size() >= 6000 || NodeVisitCount.size() >= 6000) &&
          InEdges.size() > 2 && !IsOperatorExprName(Current->Name)) {
        return false;
      }
      if (MatchedCallIdx < 0 && e.Kind != EdgeKind::Implicit) {
        if (InEdges.size() > 32 && e.Loc.File != Current->Loc.File &&
            (!FlowLoc.IsValid() || e.Loc.File != FlowLoc.File) &&
            UnmatchedExpanded >= 24) {
          return false;
        }
        if (++UnmatchedExpanded > 48) {
          return false;
        }
      }
    }
    if (IsDeadExprSource(Source)) return false;
    NextStackPtr = &CallStack;
    if (MatchedCallIdx >= 0) {
      if (e.Kind == EdgeKind::ArgPass) {
        if (e.Loc.File != MatchedArgLoc.File ||
            e.Loc.Line != MatchedArgLoc.Line ||
            e.Loc.Column != MatchedArgLoc.Column) {
          return false;
        }
        ModifiedStack.assign(CallStack.begin(),
                             CallStack.begin() + MatchedCallIdx);
        NextStackPtr = &ModifiedStack;
      } else if (e.Kind == EdgeKind::Implicit) {
        if (!NodeHasExactArgPass(Db, Current->ID, e.From, MatchedArgLoc)) {
          return false;
        }
      } else {
        return false;
      }
    } else if (e.Kind == EdgeKind::Return &&
               Current->Name.find("_return") == std::string::npos &&
               Source->Name.find("_return") != std::string::npos) {
      SourceLocation CallSiteLoc = e.Loc.IsValid() ? e.Loc : Current->Loc;
      if (CallSiteLoc.IsValid()) {
        if (CallStack.size() >= 32) {
          ModifiedStack.assign(CallStack.end() - 31, CallStack.end());
        } else {
          ModifiedStack = CallStack;
        }
        ModifiedStack.push_back(CallSiteLoc);
        NextStackPtr = &ModifiedStack;
      }
    }
    return true;
  };

  if (IsElidableNode(Current)) {
    VisitedDepths[Current->ID] = Depth;
    int SubtreeMinDepth = std::numeric_limits<int>::max();
    bool ReachedValidSource = false;
    for (const auto& e : InEdges) {
      if (e.From != Current->ID) {
        const Node* Source = Db.GetNode(e.From);
        if (!Source) continue;
        std::vector<SourceLocation> ModifiedStack;
        const std::vector<SourceLocation>* NextStackPtr = &CallStack;
        if (!ShouldTraverseEdge(e, Source, ModifiedStack, NextStackPtr)) {
          continue;
        }
        int ChildMinDepth = std::numeric_limits<int>::max();
        if (FindLeafNodes(Source, e.Loc, BackwardReachable, ForwardAdj,
                          BackwardSources, MemoizedValidUpstream,
                          NodeVisitCount, VisitedDepths, Depth, ChildMinDepth,
                          LeafNodes, *NextStackPtr)) {
          AddForwardEdge(Source->ID, Current->ID, e.Kind, e.Loc);
          ReachedValidSource = true;
        }
        SubtreeMinDepth = std::min(SubtreeMinDepth, ChildMinDepth);
      }
    }
    VisitedDepths.erase(Current->ID);
    if (Current->Kind == NodeKind::Parameter && Current->Name != "args" &&
        SubtreeMinDepth >= Depth) {
      if ((!ReachedValidSource &&
           (Depth == 0 ||
            (IsDeadParameterNode(Db, Current) &&
             !HasLiveCoLocatedParameter(Db, Current)))) ||
          (Depth == 0 && MatchedCallIdx < 0 &&
           IsApiExposedNode(Db, Current))) {
        LeafNodes.insert(Current->ID);
        ReachedValidSource = true;
      }
    }
    MemoizedValidUpstream[MemoKey] = ReachedValidSource;
    MemoizedValidUpstream[Current->ID] = ReachedValidSource;
    MinReachedDepth = std::min(MinReachedDepth, SubtreeMinDepth);
    return ReachedValidSource;
  }

  VisitedDepths[Current->ID] = Depth;
  int SubtreeMinDepth = std::numeric_limits<int>::max();

  bool HasUnvisitedSource = false;
  bool HadRejectedNonReturnSource = false;
  for (const auto& e : InEdges) {
    if (e.From != Current->ID) {
      const Node* Source = Db.GetNode(e.From);
      if (!Source) continue;
      std::vector<SourceLocation> ModifiedStack;
      const std::vector<SourceLocation>* NextStackPtr = &CallStack;
      if (!ShouldTraverseEdge(e, Source, ModifiedStack, NextStackPtr)) {
        continue;
      }
      int ChildMinDepth = std::numeric_limits<int>::max();
      if (FindLeafNodes(Source, e.Loc, BackwardReachable, ForwardAdj,
                        BackwardSources, MemoizedValidUpstream, NodeVisitCount,
                        VisitedDepths, Depth + 1, ChildMinDepth, LeafNodes,
                        *NextStackPtr)) {
        AddForwardEdge(Source->ID, Current->ID, e.Kind, e.Loc);
        HasUnvisitedSource = true;
      } else if (Source->Name.find("_return") == std::string::npos) {
        HadRejectedNonReturnSource = true;
      }
      SubtreeMinDepth = std::min(SubtreeMinDepth, ChildMinDepth);
    }
  }

  VisitedDepths.erase(Current->ID);
  MinReachedDepth = std::min(MinReachedDepth, SubtreeMinDepth);

  if (!HasUnvisitedSource) {
    // If this node has no external upstream source and only loops back to an
    // active ancestor at depth < Depth, or is a dead expression artifact whose
    // only incoming edges were empty elidable nodes, or is an uncalled/pattern
    // template parameter that has a live co-located specialization parameter,
    // return false.
    bool IsDeadCoLocatedParam =
        (Depth > 0 && Current->Kind == NodeKind::Parameter &&
         IsDeadParameterNode(Db, Current) &&
         HasLiveCoLocatedParameter(Db, Current));
    bool IsDeadDownstreamOfRejectedParam =
        (Depth > 0 && HadRejectedNonReturnSource &&
         !(Current->Kind == NodeKind::Parameter &&
           IsDeadParameterNode(Db, Current) &&
           !HasLiveCoLocatedParameter(Db, Current)));
    if (SubtreeMinDepth < Depth || (Depth > 0 && IsDeadExprNode(Current)) ||
        IsDeadCoLocatedParam || IsDeadDownstreamOfRejectedParam) {
      MemoizedValidUpstream[MemoKey] = false;
      MemoizedValidUpstream[Current->ID] = false;
      return false;
    }
    LeafNodes.insert(Current->ID);
  } else if (MatchedCallIdx < 0 && IsApiExposedNode(Db, Current)) {
    LeafNodes.insert(Current->ID);
  }
  MemoizedValidUpstream[MemoKey] = true;
  MemoizedValidUpstream[Current->ID] = true;
  return true;
}

void QueryEngine::TraverseOutgoingPruned(
    const Node* N, const Node* Target, EdgeKind FlowKind,
    const SourceLocation& FlowLoc,
    std::unordered_map<Node::Id, int>& VisitedDepths, int Depth,
    int& MinReachedDepth, const std::unordered_set<Node::Id>& BackwardReachable,
    const std::unordered_map<Node::Id, std::vector<Edge>>& ForwardAdj,
    const std::unordered_map<Node::Id, ValueRange>& NodeRanges,
    const std::unordered_map<Node::Id, HeapTaint>& NodeTaints,
    const std::unordered_map<Node::Id, bool>& NodeDead,
    const std::set<Node::Id>& LeafNodes,
    std::unordered_map<Node::Id, std::shared_ptr<QueryResultNode>>& Memo,
    std::vector<std::shared_ptr<QueryResultNode>>& OutputChildren) {
  if (!N || (Depth > 0 && IsBogusArtifactNode(Db, N))) return;

  // O(1) reachability check: N can reach Target iff N is in BackwardReachable!
  if (BackwardReachable.find(N->ID) == BackwardReachable.end()) {
    return;
  }

  auto visit_it = VisitedDepths.find(N->ID);
  if (visit_it != VisitedDepths.end()) {
    MinReachedDepth = std::min(MinReachedDepth, visit_it->second);
    return;
  }

  static const std::vector<Edge> EmptyEdges;
  auto adj_it = ForwardAdj.find(N->ID);
  const std::vector<Edge>& OutEdges =
      (adj_it != ForwardAdj.end()) ? adj_it->second : EmptyEdges;

  bool IsLeafRoot = (Depth == 0 && LeafNodes.find(N->ID) != LeafNodes.end());
  if (N->ID != Target->ID && !IsLeafRoot && IsElidableNode(N)) {
    auto elide_it = Memo.find(N->ID);
    if (elide_it != Memo.end()) {
      for (const auto& C : elide_it->second->Children) {
        auto Cloned = std::make_shared<QueryResultNode>(*C);
        if (Cloned->FlowKind == EdgeKind::Implicit &&
            FlowKind != EdgeKind::Unknown &&
            FlowKind != EdgeKind::Implicit) {
          Cloned->FlowKind = FlowKind;
          if (FlowLoc.IsValid()) Cloned->FlowLoc = FlowLoc;
        }
        OutputChildren.push_back(std::move(Cloned));
      }
      return;
    }

    VisitedDepths[N->ID] = Depth;
    int SubtreeMinDepth = std::numeric_limits<int>::max();
    std::vector<std::shared_ptr<QueryResultNode>> ElidedChildren;
    for (const auto& e : OutEdges) {
      const Node* ChildNode = Db.GetNode(e.To);
      if (ChildNode) {
        int ChildMinDepth = std::numeric_limits<int>::max();
        TraverseOutgoingPruned(ChildNode, Target, e.Kind, e.Loc, VisitedDepths,
                               Depth, ChildMinDepth, BackwardReachable,
                               ForwardAdj, NodeRanges, NodeTaints, NodeDead,
                               LeafNodes, Memo, ElidedChildren);
        SubtreeMinDepth = std::min(SubtreeMinDepth, ChildMinDepth);
      }
    }
    VisitedDepths.erase(N->ID);
    DeduplicateChildrenByNodeId(Db, ElidedChildren);

    if (!ElidedChildren.empty() || SubtreeMinDepth >= Depth) {
      auto Holder = std::make_shared<QueryResultNode>();
      Holder->Children = ElidedChildren;
      Memo[N->ID] = Holder;
    }

    for (auto& C : ElidedChildren) {
      auto Cloned = std::make_shared<QueryResultNode>(*C);
      if (Cloned->FlowKind == EdgeKind::Implicit &&
          FlowKind != EdgeKind::Unknown &&
          FlowKind != EdgeKind::Implicit) {
        Cloned->FlowKind = FlowKind;
        if (FlowLoc.IsValid()) Cloned->FlowLoc = FlowLoc;
      }
      OutputChildren.push_back(std::move(Cloned));
    }
    MinReachedDepth = std::min(MinReachedDepth, SubtreeMinDepth);
    return;
  }

  auto memo_it = Memo.find(N->ID);
  if (memo_it != Memo.end()) {
    if (!memo_it->second) {
      return;
    }
    auto CachedChild = std::make_shared<QueryResultNode>();
    CachedChild->TargetNode = N;
    CachedChild->FlowKind = FlowKind;
    CachedChild->FlowLoc = FlowLoc;
    CachedChild->Children = memo_it->second->Children;
    CachedChild->Range = memo_it->second->Range;
    CachedChild->Taint = memo_it->second->Taint;
    CachedChild->IsDead = memo_it->second->IsDead;
    OutputChildren.push_back(std::move(CachedChild));
    return;
  }

  VisitedDepths[N->ID] = Depth;
  int SubtreeMinDepth = std::numeric_limits<int>::max();

  auto Child = std::make_shared<QueryResultNode>();
  Child->TargetNode = N;
  Child->FlowKind = FlowKind;
  Child->FlowLoc = FlowLoc;
  auto range_it = NodeRanges.find(N->ID);
  Child->Range = (range_it != NodeRanges.end()) ? range_it->second
                                                : GetIntrinsicRange(N);
  auto taint_it = NodeTaints.find(N->ID);
  Child->Taint = (taint_it != NodeTaints.end()) ? taint_it->second
                                                : GetIntrinsicHeapTaint(N);
  auto dead_it = NodeDead.find(N->ID);
  Child->IsDead = (dead_it != NodeDead.end()) ? dead_it->second
                                              : IsDeadParameterNode(Db, N);

  if (N->ID != Target->ID) {
    for (const auto& e : OutEdges) {
      const Node* ChildNode = Db.GetNode(e.To);
      if (ChildNode) {
        int ChildMinDepth = std::numeric_limits<int>::max();
        TraverseOutgoingPruned(ChildNode, Target, e.Kind, e.Loc, VisitedDepths,
                               Depth + 1, ChildMinDepth, BackwardReachable,
                               ForwardAdj, NodeRanges, NodeTaints, NodeDead,
                               LeafNodes, Memo, Child->Children);
        SubtreeMinDepth = std::min(SubtreeMinDepth, ChildMinDepth);
      }
    }
  }
  DeduplicateChildrenByNodeId(Db, Child->Children, N);

  VisitedDepths.erase(N->ID);

  // Prune dead-end cycle branches whose only path to Target went through an
  // already-visited ancestor on the active forward path.
  if (N->ID != Target->ID && Child->Children.empty()) {
    if (SubtreeMinDepth >= Depth) {
      Memo[N->ID] = nullptr;
    }
    MinReachedDepth = std::min(MinReachedDepth, SubtreeMinDepth);
    return;
  }

  Memo[N->ID] = Child;
  MinReachedDepth = std::min(MinReachedDepth, SubtreeMinDepth);
  OutputChildren.push_back(std::move(Child));
}

}  // namespace dfg
