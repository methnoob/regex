#include <stdio.h>
#include <cstdint>
#include <cstring>

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
	TYPE_CHAR_CLASS_WITH_LENGTH = 2,
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

enum ParseCharacterClassState
{
	NOT_STARTED_CHAR_CLASS,
	OPENING_BRACKET_GET_CHAR_CLASS,
	NORMAL_PARSING_CHAR_CLASS,
	CLOSING_BRACKET_GET_CHAR_CLASS
};

struct parse_custom_length_result
{
	bool hasError;
	uint32_t charsConsumed;
	uint32_t minMatches;
	uint32_t maxMatches;
};

struct interval
{
	uint8_t min;
	uint8_t max;
};

struct regex_node
{
	uint8_t type;

	// for a single char
	char comparisonChar;

	// character class
	bool isNegativeClass;
	uint32_t numIntervals;
	interval characterRangeIntervals[256];

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

struct parse_character_class_result
{
	bool hasError;
	uint32_t charsConsumed;
	bool isNegativeClass;
	uint32_t numIntervals;
	interval characterRangeIntervals[256];
};

struct three_char_stack
{
	bool hasLower;
	char lower;
	bool hasRange;
	char upper;
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

	if (regexNode->type == RegexType::TYPE_REGULAR_CHAR_WITH_LENGTH) {
		printf(
			"REGEX NODE: {type: %d, comparisonChar: %c, minMatches: %d, maxMatches: %d, numMatches: %d} \n",
			(int)regexNode->type, regexNode->comparisonChar, (int)regexNode->minMatches, (int)regexNode->maxMatches, (int)regexNode->numMatches
		);
	}

	if (regexNode->type == RegexType::TYPE_CHAR_CLASS_WITH_LENGTH) {
		printf(
			"REGEX NODE: {type: %d, isNegativeClass: %d, numIntervals: %d, minMatches: %d, maxMatches: %d, numMatches: %d} \n",
			(int)regexNode->type, (int)regexNode->isNegativeClass, (int)regexNode->numIntervals, (int)regexNode->minMatches, (int)regexNode->maxMatches, (int)regexNode->numMatches
		);

		for (int i = 0; i < regexNode->numIntervals; ++i) {
			interval *rangeInterval = &regexNode->characterRangeIntervals[i];
			printf("[%c, %c]\n", (char)rangeInterval->min, (char)rangeInterval->max);
		}
}
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

	while (continueLoop && (*patternRef || parseLengthState == ParseCustomLengthState::CLOSING_BRACKET_GET)) {
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

	if (parseLengthState != ParseCustomLengthState::CLOSING_BRACKET_GET) {
		result.hasError = true;
		printf("Parsing length could not complete\n");
	}
	return result;
}

bool addCharacterClassRange(parse_character_class_result *result, three_char_stack *stack)
{
	printf("i am call. hasLower: %d, lower: %d, hasRange: %d, upper: %d\n", (int)stack->hasLower, (int)stack->lower, (int)stack->hasRange, (int)stack->upper);

	if (!stack->hasLower) {
		printf("Stack given when it has no lower item\n");
		return false;
	}

	if (!stack->hasRange) {
		stack->hasRange = true;
		stack->upper = stack->lower;
	}

	if (stack->upper < stack->lower) {
		printf("range end is smaller than range start\n");
		return false;
	}
	interval characterRangeInterval = {(uint8_t)stack->lower, (uint8_t)stack->upper};
	result->characterRangeIntervals[result->numIntervals++] = characterRangeInterval;
	return true;
}

parse_character_class_result parseCharacterClass(char *pattern, int index, regex_state_machine *stateMachine)
{
	char currentChar = pattern[index];
	parse_character_class_result resultWew = {};
	parse_character_class_result *result = &resultWew;

	if (currentChar != '[') {
		return resultWew;
	}
	bool shouldContinue = true;
	char *patternRef = pattern + index;
	uint8_t parsingState = ParseCharacterClassState::NOT_STARTED_CHAR_CLASS;
	three_char_stack charsStack = {};

	// todo: handle special stuff like \d etc here as well. also, \] is not counted as a closing bracket
	while (shouldContinue && (*patternRef || parsingState == ParseCharacterClassState::CLOSING_BRACKET_GET_CHAR_CLASS)) {
		printf("state: %d. '%s', consumed: %d, currentChar: %c\n", (int)parsingState, patternRef, (int)result->charsConsumed, patternRef[0]);
		currentChar = patternRef[0];

		switch (parsingState) {
			case ParseCharacterClassState::NOT_STARTED_CHAR_CLASS: {
				if (*patternRef != '[') {
					result->hasError = true;
					printf("No opening bracket ([) found\n");
					return resultWew;
				}
				++patternRef;
				++result->charsConsumed;
				parsingState = ParseCharacterClassState::OPENING_BRACKET_GET_CHAR_CLASS;
			} break;

			case ParseCharacterClassState::OPENING_BRACKET_GET_CHAR_CLASS: {
				if (*patternRef == '^') {
					result->isNegativeClass = true;
				} else { // -, ] are special if at the very beginning
					charsStack.hasLower = true;
					charsStack.lower = *patternRef;
				}
				++patternRef;
				++result->charsConsumed;
				parsingState = ParseCharacterClassState::NORMAL_PARSING_CHAR_CLASS;
			} break;

			case ParseCharacterClassState::NORMAL_PARSING_CHAR_CLASS: {
				if (*patternRef == ']') {
					parsingState = ParseCharacterClassState::CLOSING_BRACKET_GET_CHAR_CLASS;
					++patternRef;
					++result->charsConsumed;
					break;					
				}
				char nextChar = patternRef[1];

				if (currentChar == '-' && nextChar && nextChar != ']') {
					if (!charsStack.hasLower) { // handles cases like [a-z-c], where the z is already in a previous range
						goto addCurrentChar;
					}
					charsStack.hasRange = true;
					charsStack.upper = nextChar;

					patternRef += 2;
					result->charsConsumed += 2;

					if (!addCharacterClassRange(result, &charsStack)) {
						result->hasError = true;
						printf("failed to add character class range\n");
						return resultWew;
					}
					charsStack = three_char_stack{};
					break;
				}

				if (charsStack.hasLower) { // either a full range or a single char, depending on the if above
					if (!addCharacterClassRange(result, &charsStack)) {
						result->hasError = true;
						printf("failed to add character class range\n");
						return resultWew;
					}
					charsStack = three_char_stack{};
				}
				addCurrentChar:
				charsStack.hasLower = true;
				charsStack.lower = currentChar;
				++patternRef;
				++result->charsConsumed;
			} break;

			case ParseCharacterClassState::CLOSING_BRACKET_GET_CHAR_CLASS: {
				if (charsStack.hasLower && !addCharacterClassRange(result, &charsStack)) {
					result->hasError = true;
					printf("failed to add character class range\n");
					return resultWew;
				}
				charsStack = three_char_stack{};
				shouldContinue = false;
			} break;

			default: {
				result->hasError = true;
				printf("Reached impossible state: %d\n", (int)parsingState);
				return resultWew;
			}
		};
	}

	if (parsingState != ParseCharacterClassState::CLOSING_BRACKET_GET_CHAR_CLASS) {
		result->hasError = true;
		printf("Reached impossible state: %d\n", (int)parsingState);
	}
	return resultWew;
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

	regex_state_machine errorMachine = {};
	errorMachine.hasError = true;

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
			continue;
		}
		parse_character_class_result characterClassResult = parseCharacterClass(pattern, i, &stateMachine);

		if (characterClassResult.hasError) {
			return errorMachine;
		}

		if (characterClassResult.charsConsumed > 0) {
			node = regex_node{
				.type = RegexType::TYPE_CHAR_CLASS_WITH_LENGTH, 
				.isNegativeClass = characterClassResult.isNegativeClass,
				.numIntervals = characterClassResult.numIntervals,
				.minMatches = 1,
				.maxMatches = 1,
			};
			memcpy(
			    node.characterRangeIntervals,
			    characterClassResult.characterRangeIntervals,
			    sizeof(node.characterRangeIntervals)
			);
			int onePastLengthQuantifier = i + characterClassResult.charsConsumed;
			i = onePastLengthQuantifier - 1; // ++i when the loop ends would skip a char otherwise
		} else {
			node = regex_node{
				.type = RegexType::TYPE_REGULAR_CHAR_WITH_LENGTH, 
				.comparisonChar = pattern[i], 
				.minMatches = 1, 
				.maxMatches = 1
			};
		}
		addNodeToStateMachine(&node, &stateMachine);
	}
	printStateMachine(&stateMachine);
	return stateMachine;
}

match_result doesNodeMatch(regex_node *regexNode, char *testString) {
	match_result result = {};

	if (*testString == 0) {
		return result;
	}
	char currentChar = testString[0];

	if (regexNode->type == RegexType::TYPE_REGULAR_CHAR_WITH_LENGTH) {
		if (currentChar == regexNode->comparisonChar) {
			result.matched = true;
		}
	}

	if (regexNode->type == RegexType::TYPE_CHAR_CLASS_WITH_LENGTH) {
		bool matchedInterval = false;

		for (uint32_t i = 0; i < regexNode->numIntervals; ++i) {
			interval *characterRangeInterval = &regexNode->characterRangeIntervals[i];

			if (characterRangeInterval->min <= currentChar && currentChar <= characterRangeInterval->max) {
				matchedInterval = true;
				break;
			}
		}
		// if it is negative, we want it to match 0 intervals. otherwise, even one match is good
		result.matched = regexNode->isNegativeClass ? !matchedInterval : matchedInterval;
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
		// char pattern[] = "a{1,}bc{,1}c{1,1}";
	// char pattern[] = "[a-z]+";
	// char pattern[] = "[]-a-z]+";
	char pattern[] = "[^a-c-f]+";
	char searchLines[][100] = {
		"abcde",
		"ab",
		"abcabcd",
		"aaaaabcaaabcaaaaaaaaaaaaab",
		"-]"
	};
	int numLines = sizeof(searchLines) / sizeof(*searchLines);
	int patternLength = strLen(pattern);
	regex_state_machine stateMachine = parseRegex(pattern);

	for (int i = 0; i < numLines; ++i) {
		processString(searchLines[i], &stateMachine);
	}
	return 0;
}