#include <stdio.h>
#include <cstdint>
#include "arena.cpp"
#include "my_string.cpp"
#include "regex.h"

static memory_arena GlobalArena;
static char metaCharacters[] = "dDsSwWN";

int strLen(const char *str)
{
	int length = 0;

	while (*str) {
		++length;
		++str;
	}
	return length;	
}

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
			"REGEX NODE: {type: %d, comparisonChar: %c, minMatches: %d, maxMatches: %d, numMatches: %d, isGreedy: %d, hasLengthSpecified: %d} \n",
			(int)regexNode->type, regexNode->comparisonChar, (int)regexNode->minMatches, (int)regexNode->maxMatches, (int)regexNode->numMatches, (int)regexNode->isGreedy, (int)regexNode->hasLengthSpecified
		);
	}

	if (regexNode->type == RegexType::TYPE_CHAR_CLASS_WITH_LENGTH) {
		printf(
			"REGEX NODE: {type: %d, isNegativeClass: %d, numIntervals: %d, minMatches: %d, maxMatches: %d, numMatches: %d, isGreedy: %d, hasLengthSpecified: %d} \n",
			(int)regexNode->type, (int)regexNode->isNegativeClass, (int)regexNode->numIntervals, (int)regexNode->minMatches, (int)regexNode->maxMatches, (int)regexNode->numMatches, (int)regexNode->isGreedy, (int)regexNode->hasLengthSpecified
		);

		for (int i = 0; i < regexNode->numIntervals; ++i) {
			interval *rangeInterval = &regexNode->characterRangeIntervals[i];
			printf("[%c, %c] (= [%d, %d])\n", (char)rangeInterval->min, (char)rangeInterval->max, rangeInterval->min, rangeInterval->max);
		}
	}
}

void printStateMachine(regex_state_machine *stateMachine) {
	printf("-----------------State Machine Start-----------------\n\n");
	printf("Original pattern: %s\n\n", stateMachine->originalPattern->cstr);

	for (uint32_t i = 0; i < stateMachine->numNodes; ++i) {
		printRegexNode(&stateMachine->regexNodes[i]);
	}
	printf("\n-----------------State Machine End-------------------\n\n");
}

parse_custom_length_result parseCustomLength(my_string *pattern, int openingBracketIndex) {
	parse_custom_length_result result = {};
	uint8_t parseLengthState = ParseCustomLengthState::NOT_STARTED;
	int currentIndex = openingBracketIndex;
	bool continueLoop = true;
	bool wasMaxInitialized = false;

	while (continueLoop && (currentIndex < pattern->length || parseLengthState == ParseCustomLengthState::CLOSING_BRACKET_GET)) {
		switch (parseLengthState) {
			case ParseCustomLengthState::NOT_STARTED: {
				if (charAt(pattern, currentIndex) == '{') {
					parseLengthState = ParseCustomLengthState::OPENING_BRACKET_GET;
					++result.charsConsumed;
					++currentIndex;
				} else {
					result.hasError = true;
					printf("No opening bracket found. '%s' at %d\n", pattern->cstr, openingBracketIndex);
					return result;
				}
			} break;

			case ParseCustomLengthState::OPENING_BRACKET_GET: {
				int skipped = skipConsecutiveSpaces(pattern, currentIndex);
				currentIndex += skipped;
				result.charsConsumed += skipped;
				parseLengthState = ParseCustomLengthState::FIRST_NUM_START;
			} break;

			case ParseCustomLengthState::FIRST_NUM_START: {
				char currentChar = charAt(pattern, currentIndex);

				if (48 <= currentChar && currentChar <= 57) {
					result.minMatches = 10 * result.minMatches + ((uint8_t)currentChar - 48);
					++result.charsConsumed;
					++currentIndex;
				} else if (currentChar == ' ' || currentChar == ',') {
					parseLengthState = ParseCustomLengthState::FIRST_NUM_GET;
				} else {
					result.hasError = true;
					printf("Expected digit only, got: %c\n", currentChar);
					return result;
				}
			} break;

			case ParseCustomLengthState::FIRST_NUM_GET: {
				int skipped = skipConsecutiveSpaces(pattern, currentIndex);
				currentIndex += skipped;
				result.charsConsumed += skipped;
				char currentChar = charAt(pattern, currentIndex);

				if (currentChar == ',') {
					parseLengthState = ParseCustomLengthState::COMMA_GET;
					++currentIndex;
					++result.charsConsumed;
				} else {
					result.hasError = true;
					printf("Expected comma, got: %c\n", currentChar);
					return result;
				}
			} break;

			case ParseCustomLengthState::COMMA_GET: {
				int skipped = skipConsecutiveSpaces(pattern, currentIndex);
				currentIndex += skipped;
				result.charsConsumed += skipped;
				parseLengthState = ParseCustomLengthState::SECOND_NUM_START;
			} break;

			case ParseCustomLengthState::SECOND_NUM_START: {
				char currentChar = charAt(pattern, currentIndex);

				if (48 <= currentChar && currentChar <= 57) {
					wasMaxInitialized = true;
					result.maxMatches = 10 * result.maxMatches + ((uint8_t)currentChar - 48);
					++result.charsConsumed;
					++currentIndex;
				} else if (currentChar == ' ' || currentChar == '}') {
					parseLengthState = ParseCustomLengthState::SECOND_NUM_GET;
				} else {
					result.hasError = true;
					printf("Expected digit only, got: %c\n", currentChar);
					return result;
				}
			} break;

			case ParseCustomLengthState::SECOND_NUM_GET: {
				int skipped = skipConsecutiveSpaces(pattern, currentIndex);
				currentIndex += skipped;
				result.charsConsumed += skipped;
				char currentChar = charAt(pattern, currentIndex);

				if (currentChar == '}') {
					parseLengthState = ParseCustomLengthState::CLOSING_BRACKET_GET;
					++currentIndex;
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
	// printf("i am call. hasLower: %d, lower: %d, hasRange: %d, upper: %d\n", (int)stack->hasLower, (int)stack->lower, (int)stack->hasRange, (int)stack->upper);

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

	if (result->numIntervals >= 255) {
		printf("interval array length exceeded\n");
	}
	result->characterRangeIntervals[result->numIntervals++] = characterRangeInterval;
	return true;
}

parse_character_class_result parseCharacterClass(my_string *pattern, int index, regex_state_machine *stateMachine)
{
	char currentChar = charAt(pattern, index);
	parse_character_class_result resultWew = {};

	if (currentChar != '[') {
		return resultWew;
	}
	parse_character_class_result *result = &resultWew;
	result->characterRangeIntervals = PushArray(&GlobalArena, 256, interval);
	bool shouldContinue = true;
	int currentIndex = index;
	uint8_t parsingState = ParseCharacterClassState::NOT_STARTED_CHAR_CLASS;
	three_char_stack charsStack = {};

	// todo: handle special stuff like \d etc here as well. also, \] is not counted as a closing bracket
	while (shouldContinue && (currentIndex < pattern->length || parsingState == ParseCharacterClassState::CLOSING_BRACKET_GET_CHAR_CLASS)) {
		// printf("state: %d. '%s', consumed: %d, currentChar: %c\n", (int)parsingState, patternRef, (int)result->charsConsumed, patternRef[0]);
		currentChar = charAt(pattern, currentIndex);

		switch (parsingState) {
			case ParseCharacterClassState::NOT_STARTED_CHAR_CLASS: {
				if (currentChar != '[') {
					result->hasError = true;
					printf("No opening bracket ([) found\n");
					return resultWew;
				}
				++currentIndex;
				++result->charsConsumed;
				parsingState = ParseCharacterClassState::OPENING_BRACKET_GET_CHAR_CLASS;
			} break;

			case ParseCharacterClassState::OPENING_BRACKET_GET_CHAR_CLASS: {
				if (currentChar == '^') {
					result->isNegativeClass = true;
				} else { // -, ] are special if at the very beginning
					charsStack.hasLower = true;
					charsStack.lower = currentChar;
				}
				++currentIndex;
				++result->charsConsumed;
				parsingState = ParseCharacterClassState::NORMAL_PARSING_CHAR_CLASS;
			} break;

			case ParseCharacterClassState::NORMAL_PARSING_CHAR_CLASS: {
				if (currentChar == ']') {
					parsingState = ParseCharacterClassState::CLOSING_BRACKET_GET_CHAR_CLASS;
					++currentIndex;
					++result->charsConsumed;
					break;					
				}
				char nextChar = charAt(pattern, currentIndex + 1);

				if (currentChar == '-' && nextChar && nextChar != ']') {
					if (!charsStack.hasLower) { // handles cases like [a-z-c], where the z is already in a previous range
						goto addCurrentChar;
					}
					charsStack.hasRange = true;
					charsStack.upper = nextChar;

					currentIndex += 2;
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
				++currentIndex;
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

parse_custom_length_result parseLengthQuantifier(my_string *pattern, int index, regex_state_machine *stateMachine)
{
	char currentChar = charAt(pattern, index);
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
	int nextCharIndex = index + parseResult.charsConsumed;
	bool wasLengthQuantifierParsed = !parseResult.hasError && parseResult.charsConsumed > 0;

	if (wasLengthQuantifierParsed && doesCharAtIndexMatchTestChar(pattern, nextCharIndex, '+')) {
		printf("extra plus found weeeeeeeee\n");
		++parseResult.charsConsumed;
		parseResult.isGreedy = true;
	}
	return parseResult;
}

parse_special_character_result parseSpecialCharacter(my_string *pattern, int index, CharContext charContext)
{
	const char *escapableCharacters;
	parse_special_character_result result = {};

	switch (charContext) {
		case CharContext::CharContext_Regular: {
			escapableCharacters = "()|.*+?{[0\\";

			if (doesCharAtIndexMatchTestChar(pattern, index, '.')) {
				result.specialChar = '.';
				result.charType = SpecialChar::SpecialChar_Meta;
				result.charsConsumed = 1;
				return result;
			}
		} break;

		case CharContext::CharContext_CharacterClass: {
			escapableCharacters = "]^0\\";
		} break;

		default: {
			printf("invalid char context given: %d\n", (int)charContext);
			result.hasError = true;
			return result;
		}
	}

	if (!doesCharAtIndexMatchTestChar(pattern, index, '\\')) {
		return result;
	}

	for (int i = 0; i < strLen(metaCharacters); ++i) {
		if (doesCharAtIndexMatchTestChar(pattern, index + 1, metaCharacters[i])) {
			result.specialChar = metaCharacters[i];
			result.charType = SpecialChar::SpecialChar_Meta;
			result.charsConsumed = 2;
			return result;
		}
	}

	for (int i = 0; i < strLen(escapableCharacters); ++i) {
		if (doesCharAtIndexMatchTestChar(pattern, index + 1, escapableCharacters[i])) {
			result.specialChar = escapableCharacters[i];
			result.charType = SpecialChar::SpecialChar_Literal;
			result.charsConsumed = 2;
			return result;
		}
	}
	char nextChar = charAt(pattern, index + 1);
	printf("unrecognized / empty char escape: %c (%d)\n", nextChar, (int)nextChar);
	result.hasError = true;
	return result;
}

regex_state_machine parseRegex(my_string *pattern)
{
	regex_state_machine stateMachine = {};
	stateMachine.originalPattern = pattern;
	stateMachine.regexNodes = PushArray(&GlobalArena, 256, regex_node);

	regex_state_machine errorMachine = {};
	errorMachine.hasError = true;

	for (int i = 0; i < pattern->length; ++i) {
		regex_node node;
		regex_node *previousNode = stateMachine.numNodes == 0 ? NULL : &stateMachine.regexNodes[stateMachine.numNodes - 1];

		char currentChar = charAt(pattern, i);
		// todo: escaped / special chars
		// todo: +/-ive lookahead/backs
		// todo: groups :skull:

		parse_custom_length_result lengthResult = parseLengthQuantifier(pattern, i, &stateMachine);

		if (lengthResult.hasError) {
			return errorMachine;
		}
		// printf("current char: %c, next char: %c, consumed: %d\n", charAt(pattern, i), nextChar, lengthResult.charsConsumed);

		if (lengthResult.charsConsumed > 0) {
			if (!previousNode || previousNode->hasLengthSpecified) {
				printf("No previous item for length specification, or the previous node has already been given a length\n");
				regex_state_machine errorMachine = {};
				errorMachine.hasError = true;
			}
			previousNode->minMatches = lengthResult.minMatches; 
			previousNode->maxMatches = lengthResult.maxMatches;
			previousNode->isGreedy = lengthResult.isGreedy;
			previousNode->hasLengthSpecified = true;

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
			node.characterRangeIntervals = characterClassResult.characterRangeIntervals;
			int onePastLengthQuantifier = i + characterClassResult.charsConsumed;
			i = onePastLengthQuantifier - 1; // ++i when the loop ends would skip a char otherwise
		} else {
			parse_special_character_result specialCharResult = parseSpecialCharacter(pattern, i, CharContext_Regular);

			if (specialCharResult.hasError) {
				return errorMachine;
			}

			if (specialCharResult.charsConsumed > 0) {
				if (specialCharResult.charType == SpecialChar::SpecialChar_Literal) {
					node = regex_node{
						.type = RegexType::TYPE_REGULAR_CHAR_WITH_LENGTH, 
						.comparisonChar = specialCharResult.specialChar, 
						.minMatches = 1, 
						.maxMatches = 1
					};
				} else {
					node = regex_node{
						.type = RegexType::TYPE_META_CHAR_WITH_LENGTH, 
						.comparisonChar = specialCharResult.specialChar, 
						.minMatches = 1, 
						.maxMatches = 1
					};
				}
				int onePastLengthQuantifier = i + specialCharResult.charsConsumed;
				i = onePastLengthQuantifier - 1; // ++i when the loop ends would skip a char otherwise
			} else {
				node = regex_node{
					.type = RegexType::TYPE_REGULAR_CHAR_WITH_LENGTH, 
					.comparisonChar = currentChar, 
					.minMatches = 1, 
					.maxMatches = 1
				};
			}
		}
		addNodeToStateMachine(&node, &stateMachine);
	}
	printStateMachine(&stateMachine);
	return stateMachine;
}

bool matchMetaChar(char metaChar, char testChar)
{
	bool result = false;

	switch (metaChar) {
		case '.': {
			result = testChar != '\n';
		} break;

		case 'd': {
			result = '0' <= testChar && testChar <= '9';
		} break;

		case 'D': {
			result = !('0' <= testChar && testChar <= '9');
		} break;

		case 's': {
			result = (testChar == ' ') || (testChar == '\t');
		} break;

		case 'S': {
			result = !((testChar == ' ') || (testChar == '\t'));
		} break;

		case 'w': {
			result = ('a' <= testChar && testChar <= 'z') || ('A' <= testChar && testChar <= 'Z');
		} break;

		case 'W': {
			result = !(('a' <= testChar && testChar <= 'z') || ('A' <= testChar && testChar <= 'Z'));
		} break;

		case 'N': {
			result = testChar != '\n';
		} break;

		default: {
			printf("unknown meta char: %c\n", metaChar);
		}
	}
	return result;
}

match_result doesNodeMatch(regex_node *regexNode, char *testString) {
	match_result result = {};

	if (*testString == 0) { // todo: this should be binary-safe
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

	if (regexNode->type == RegexType::TYPE_META_CHAR_WITH_LENGTH) {
		result.matched = matchMetaChar(regexNode->comparisonChar, testString[0]);
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
	return regexNode && !regexNode->isGreedy && (regexNode->numMatches > regexNode->minMatches);
}

void resetStateMachine(regex_state_machine *stateMachine) {
	for (uint32_t i = 0; i < stateMachine->numNodes; ++i) {
		stateMachine->regexNodes[i].numMatches = 0;
	}
}

void processString(my_string *testString, regex_state_machine *stateMachine)
{
	if (stateMachine->numNodes == 0) {
		return;
	}
	int matchStart = 0;
	int matchEnd = matchStart;
	char *testStringRef = testString->cstr + matchStart;

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
				testStringRef = testString->cstr + matchStart;
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
			printf("matched: '%s' from %d to %d. Leftover string: '%s'\n", testString->cstr, matchStart, matchEnd, testStringRef);

			if (matchEnd > matchStart) {				
				matchStart = matchEnd;
			} else {
				printf("vacuous(?) match, moving ahead by 1\n");
				++matchStart;
				matchEnd = matchStart;
				++testStringRef;
			}
		}
		matchedAllNodes = true;
		resetStateMachine(stateMachine);
	}
}

int main(void)
{
	uint32_t bufferSize = (uint32_t)Megabytes(20);
	uint8_t *backingMemory = new uint8_t[bufferSize];
	initializeArena(&GlobalArena, bufferSize, backingMemory);

	// char pattern[] = "a+bc?c";
		// char pattern[] = "a{1,}bc{,1}c{1,1}";
	// char pattern[] = "[a-z]+";
	// char pattern[] = "[]-a-z]+";
	// char pattern[] = "[^a-c-f]++";
	// char pattern[] = "[a-z]{0,100}+a";
	// char pattern[] = ".*c";
	// char pattern[] = "\\w+";
	// char pattern[] = "\\d+";
	char pattern[] = "\\{";
	char searchLines[][100] = {
		"abcde",
		"ab",
		"abcabcd",
		"aaaaabcaaabcaaaaaaaaaaaaab",
		"-]",
		"-]132[]11{["
	};
	int numLines = sizeof(searchLines) / sizeof(*searchLines);
	my_string patternString = fromCString(&GlobalArena, pattern, strLen(pattern));
	regex_state_machine stateMachine = parseRegex(&patternString);

	for (int i = 0; i < numLines; ++i) {
		my_string testString = fromCString(&GlobalArena, searchLines[i], strLen(searchLines[i]));
		processString(&testString, &stateMachine);
	}
	printf("\n\nmemory used: %llu bytes\n\n", GlobalArena.currentOffset);
	return 0;
}