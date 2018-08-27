
CXXFLAGS += -g -std=c++0x -Wall #-Wextra
CXXFLAGS += -I. -I../ -I../misc/ -I../tree/ -I../dfa/ \
	-I../../../../../antlrcpp/

#TODO LDFLAGS += ?

ALL_CXXFLAGS = $(CPPFLAGS) $(CXXFLAGS)
ALL_LDFLAGS = $(LDFLAGS)

# Escote's files
SRCS = \
    AbstractPredicateTransition.cpp \
	ActionTransition.cpp \
	ArrayPredictionContext.cpp \
	ATNDeserializationOptions.cpp \
	ATNDeserializer.cpp \
	ATNState.cpp \
	ATNType.cpp \
	AtomTransition.cpp \
	BasicBlockStartState.cpp \
	BasicState.cpp \
	BlockEndState.cpp \
	BlockStartState.cpp \
	DecisionState.cpp \
	EmptyPredictionContext.cpp \
	EpsilonTransition.cpp \
	LexerATNConfig.cpp \
	LoopEndState.cpp
# Escote's TODO: LL1Analyzer.cpp LexerATNSimulator.cpp ATNSimulator.cpp \
	ATNSerializer.cpp ATNConfigSet.cpp ATNConfig.cpp \
	ATN.cpp

# Alejandro's files
SRCS += \
	NotSetTransition.cpp \
	OrderedATNConfigSet.cpp \
	PlusBlockStartState.cpp \
	PlusLoopbackState.cpp \
	PredicateTransition.cpp \
	PredictionMode.cpp \
	RangeTransition.cpp \
	RuleStartState.cpp \
	RuleStopState.cpp \
	RuleTransition.cpp \
	SemanticContext.cpp \
	SetTransition.cpp \
	SingletonPredictionContext.cpp \
	StarBlockStartState.cpp \
	StarLoopbackState.cpp \
	StarLoopEntryState.cpp \
	TokensStartState.cpp \
	Transition.cpp \
	WildcardTransition.cpp
# Alejandro's TODO: PredictionContext.cpp PredictionContextCache.cpp \
	PrecedencePredicateTransition.cpp ParserATNSimulator.cpp

OBJS = $(SRCS:.cpp=.o)

all: $(OBJS)

%.o: %.cpp
	$(CXX) -c $(ALL_CXXFLAGS) $< -o $@ 

clean:
	$(RM) $(OBJS)

