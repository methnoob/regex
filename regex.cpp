#include <stdio.h>
#include <cstdint>

int strLen(char *str)
{
	int length = 0;

	while (*str) {
		++length;
		++str;
	}
	return length;	
}

enum RegexType
{
	TYPE_REGULAR_CHAR_WITH_LENGTH = 1,
};

enum ParseCustomLengthState
{
	NOT_STARTED,
	OPENING_BRACKET_GET,
	FIRST_NUM_START,
	FIRST_NUM_GET,
	COMMA_GET,
	SECOND_NUM_START,
	SECOND_NUM_GET,
	CLOSING_BRACKET_GET
};

struct parse_custom_length_result
{
	bool hasError;
	uint32_t charsConsumed;
	uint32_t minMatches;
	uint32_t maxMatches;
};

struct regex_node
{
	uint8_t type;
	char comparisonChar;
	uint32_t minMatches;
	uint32_t maxMatches;
	uint32_t numMatches;
};

struct match_result
{
	bool matched;
};

struct regex_state_machine
{
	char *originalPattern;
	bool hasError;
	uint32_t numNodes;
	regex_node regexNodes[256];
};

struct lengthQuantifierParseResult
{
	bool isValid;
	uint32_t minMathces;
	uint32_t maxMatches;
};

void addNodeToStateMachine(regex_node *regexNode, regex_state_machine *stateMachine) {
	// todo: assert?
	stateMachine->regexNodes[stateMachine->numNodes++] = *regexNode;
}

void printRegexNode(regex_node *regexNode) {
	if (!regexNode) {
		// printf("node is null\n");
		return;
	}
	printf(
		"REGEX NODE: {type: %d, comparisonChar: %c, minMatches: %d, maxMatches: %d, numMatches: %d} \n",
		(int)regexNode->type, regexNode->comparisonChar, (int)regexNode->minMatches, (int)regexNode->maxMatches, (int)regexNode->numMatches
	);
}

void printStateMachine(regex_state_machine *stateMachine) {
	printf("-----------------State Machine Start-----------------\n\n");
	printf("Original pattern: %s\n\n", stateMachine->originalPattern);

	for (uint32_t i = 0; i < stateMachine->numNodes; ++i) {
		printRegexNode(&stateMachine->regexNodes[i]);
	}
	printf("\n-----------------State Machine End-------------------\n");
}

parse_custom_length_result parseCustomLength(char *pattern, int openingBracketIndex) {
	parse_custom_length_result result = {};
	uint8_t parseLengthState = ParseCustomLengthState::NOT_STARTED;
	char *patternRef = pattern + openingBracketIndex;
	bool continueLoop = true;
	bool wasMaxInitialized = false;

	while (continueLoop && *patternRef) {
		switch (parseLengthState) {
			case ParseCustomLengthState::NOT_STARTED: {
				if (*patternRef == '{') {
					parseLengthState = ParseCustomLengthState::OPENING_BRACKET_GET;
					++result.charsConsumed;
					++patternRef;
				} else {
					result.hasError = true;
					printf("No opening bracket found. '%s' at %d\n", pattern, openingBracketIndex);
					return result;
				}
			} break;

			case ParseCustomLengthState::OPENING_BRACKET_GET: {
				while (*patternRef == ' ') { // skip white spaces
					++patternRef;
					++result.charsConsumed;
				}
				parseLengthState = ParseCustomLengthState::FIRST_NUM_START;
			} break;

			case ParseCustomLengthState::FIRST_NUM_START: {
				char currentChar = *patternRef;

				if (48 <= currentChar && currentChar <= 57) {
					result.minMatches = 10 * result.minMatches + ((uint8_t)currentChar - 48);
					++result.charsConsumed;
					++patternRef;
				} else if (currentChar == ' ' || currentChar == ',') {
					parseLengthState = ParseCustomLengthState::FIRST_NUM_GET;
				} else {
					result.hasError = true;
					printf("Expected digit only, got: %c\n", currentChar);
					return result;
				}
			} break;

			case ParseCustomLengthState::FIRST_NUM_GET: {
				while (*patternRef == ' ') { // skip white spaces
					++patternRef;
					++result.charsConsumed;
				}
				char currentChar = *patternRef;

				if (currentChar == ',') {
					parseLengthState = ParseCustomLengthState::COMMA_GET;
					++patternRef;
					++result.charsConsumed;
				} else {
					result.hasError = true;
					printf("Expected comma, got: %c\n", currentChar);
					return result;
				}
			} break;

			case ParseCustomLengthState::COMMA_GET: {
				while (*patternRef == ' ') { // skip white spaces
					++patternRef;
					++result.charsConsumed;
				}
				parseLengthState = ParseCustomLengthState::SECOND_NUM_START;
			} break;

			case ParseCustomLengthState::SECOND_NUM_START: {
				char currentChar = *patternRef;

				if (48 <= currentChar && currentChar <= 57) {
					wasMaxInitialized = true;
					result.maxMatches = 10 * result.maxMatches + ((uint8_t)currentChar - 48);
					++result.charsConsumed;
					++patternRef;
				} else if (currentChar == ' ' || currentChar == '}') {
					parseLengthState = ParseCustomLengthState::SECOND_NUM_GET;
				} else {
					result.hasError = true;
					printf("Expected digit only, got: %c\n", currentChar);
					return result;
				}
			} break;

			case ParseCustomLengthState::SECOND_NUM_GET: {
				while (*patternRef == ' ') { // skip white spaces
					++patternRef;
					++result.charsConsumed;
				}
				char currentChar = *patternRef;

				if (currentChar == '}') {
					parseLengthState = ParseCustomLengthState::CLOSING_BRACKET_GET;
					++patternRef;
					++result.charsConsumed;
				} else {
					result.hasError = true;
					printf("Expected comma, got: %c\n", currentChar);
					return result;
				}
			} break;

			case ParseCustomLengthState::CLOSING_BRACKET_GET: {
				continueLoop = false;	
			} break;

			default: {
				result.hasError = true;
				printf("Reached impossible state: %d\n", (int)parseLengthState);
				return result;
			}
		}
	}

	if (wasMaxInitialized && result.maxMatches < result.minMatches) {
		result.hasError = true;
		printf("Max is smaller than min: %d < %d\n", (int)result.maxMatches, (int)result.minMatches);
	}
	return result;
}

parse_custom_length_result parseLengthQuantifier(char *pattern, int index, regex_state_machine *stateMachine)
{
	char currentChar = pattern[index];
	parse_custom_length_result parseResult = {};

	switch (currentChar) {
		case '*': {
			parseResult.charsConsumed = 1;
		} break;

		case '+': {
			parseResult.charsConsumed = 1;
			parseResult.minMatches = 1;
		} break;

		case '?': {
			parseResult.charsConsumed = 1;
			parseResult.maxMatches = 1;
		} break;

		case '{': {
			parseResult = parseCustomLength(pattern, index);
		} break;
	};
	return parseResult;
}

regex_state_machine parseRegex(char *pattern)
{
	regex_state_machine stateMachine = {};
	stateMachine.originalPattern = pattern;
	int patternLength = strLen(pattern);

	for (int i = 0; i < patternLength; ++i) {
		regex_node node;
		regex_node *previousNode = stateMachine.numNodes == 0 ? NULL : &stateMachine.regexNodes[stateMachine.numNodes - 1];

		char currentChar = pattern[i];
		// todo: '++' prevents backtracking
		// todo: escaped / special chars
		// todo: parsing []
		// todo: groups :skull:

		parse_custom_length_result lengthResult = parseLengthQuantifier(pattern, i, &stateMachine);

		if (lengthResult.hasError) {
			regex_state_machine errorMachine = {};
			errorMachine.hasError = true;
			return errorMachine;
		}
		// printf("current char: %c, next char: %c, consumed: %d\n", pattern[i], nextChar, lengthResult.charsConsumed);

		if (lengthResult.charsConsumed > 0) {
			if (!previousNode) {
				printf("No previous item for length specification");
				regex_state_machine errorMachine = {};
				errorMachine.hasError = true;
			}
			previousNode->minMatches = lengthResult.minMatches; 
			previousNode->maxMatches = lengthResult.maxMatches;

			int onePastLengthQuantifier = i + lengthResult.charsConsumed;
			i = onePastLengthQuantifier - 1; // ++i when the loop ends would skip a char otherwise
		} else {
			node = regex_node{RegexType::TYPE_REGULAR_CHAR_WITH_LENGTH, pattern[i], 1, 1};
			addNodeToStateMachine(&node, &stateMachine);
		}
	}
	printStateMachine(&stateMachine);
	return stateMachine;
}

match_result doesNodeMatch(regex_node *regexNode, char *testString) {
	match_result result = {};

	if (regexNode->type == RegexType::TYPE_REGULAR_CHAR_WITH_LENGTH) {
		int testStringLength = strLen(testString);

		if (testStringLength <= 0) {
			return result;
		}

		if (*testString == regexNode->comparisonChar) {
			result.matched = true;
		}
	}
	return result;
}

bool areLengthNodeMatchesInRange(regex_node *regexNode) {
	return (regexNode->minMatches <= regexNode->numMatches) && 
	(
		regexNode->maxMatches == 0 || (regexNode->numMatches <= regexNode->maxMatches)
	);
}

bool areLengthNodeMatchesMaxedOut(regex_node *regexNode) {
	return regexNode->numMatches > 0 && regexNode->numMatches == regexNode->maxMatches;
}

bool canNodeBeBacktracked(regex_node *regexNode) {
	// printRegexNode(regexNode);
	return regexNode && (regexNode->numMatches > regexNode->minMatches);
}

void resetStateMachine(regex_state_machine *stateMachine) {
	for (uint32_t i = 0; i < stateMachine->numNodes; ++i) {
		stateMachine->regexNodes[i].numMatches = 0;
	}
}

void processString(char *testString, regex_state_machine *stateMachine)
{
	if (stateMachine->numNodes == 0) {
		return;
	}
	int matchStart = 0;
	int matchEnd = matchStart;
	char *testStringRef = testString + matchStart;

	while (*testStringRef) {
		bool matchedAllNodes = true;

		for (uint32_t i = 0; i < stateMachine->numNodes && matchedAllNodes; ++i) {
			regex_node *currentNode = &stateMachine->regexNodes[i];
			regex_node *previousNode = (i > 0) ? &stateMachine->regexNodes[i - 1] : NULL;
			match_result matchResult = doesNodeMatch(currentNode, testStringRef);
			// printRegexNode(currentNode);

			if (!matchResult.matched) {
				if (areLengthNodeMatchesInRange(currentNode)) {
					continue; // move to the next node
				} else if (canNodeBeBacktracked(previousNode)) {
					// printf("back tracking node: \n");
					// printRegexNode(previousNode);
					// printRegexNode(currentNode);
					// trying to backtrack the previous node
					--matchEnd;
					--testStringRef;
					--previousNode->numMatches;
					--i;
					continue;
				}
				matchedAllNodes = false;
				++matchStart; // move starting index ahead
				// reset ref and end index
				testStringRef = testString + matchStart;
				matchEnd = matchStart;
			} else {
				// move end index and ref ahead
				++matchEnd;
				++testStringRef;
				++currentNode->numMatches;
			}
			// printRegexNode(currentNode);
			// printf("string: '%s' / '%s'\n", testStringRef, testString);

			if (areLengthNodeMatchesMaxedOut(currentNode)) { // greedy
				// printf("node maxed out, moving ahead\n");
				continue; // move to the next node
			} else { // need to at least replay node till matches are in range. if ungreedy, it would be !areLengthNodeMatchesInRange
				// printf("replaying node\n");
				--i; // replay current node
			}
		}

		if (matchedAllNodes) {
			printf("matched: '%s' from %d to %d. Leftover string: '%s'\n", testString, matchStart, matchEnd, testStringRef);			
			matchStart = matchEnd;
		}
		matchedAllNodes = true;
		resetStateMachine(stateMachine);
	}
}

int main(void)
{
	// char pattern[] = "a+bc?c";
		char pattern[] = "a{1,}bc{,1}c{1,1}";
	char searchLines[][100] = {
		"abcde",
		"ab",
		"abcabcd",
		"aaaaabcaaabcaaaaaaaaaaaaab",
	};
	int numLines = sizeof(searchLines) / sizeof(*searchLines);
	int patternLength = strLen(pattern);
	regex_state_machine stateMachine = parseRegex(pattern);

	for (int i = 0; i < numLines; ++i) {
		processString(searchLines[i], &stateMachine);
	}
	return 0;
}