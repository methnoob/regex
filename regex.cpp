#include <stdio.h>
#include <cstdint>
#include "arena.cpp"
#include "my_string.cpp"
#include "regex.h"

static memory_arena GlobalArena;
static char metaCharacters[] = "dDsSwW";

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

	if (regexNode->type == RegexType_RegularChar) {
		printf(
			"REGEX NODE: {type: %d, comparisonChar: %c, minMatches: %d, maxMatches: %d, numMatches: %d, isGreedy: %d, hasLengthSpecified: %d} \n",
			(int)regexNode->type, regexNode->comparisonChar, (int)regexNode->minMatches, (int)regexNode->maxMatches, (int)regexNode->numMatches, (int)regexNode->isGreedy, (int)regexNode->hasLengthSpecified
		);
	}

	if (regexNode->type == RegexType_CharClass) {
		printf(
			"REGEX NODE: {type: %d, isNegativeClass: %d, numIntervals: %d, minMatches: %d, maxMatches: %d, numMatches: %d, isGreedy: %d, hasLengthSpecified: %d} \n",
			(int)regexNode->type, (int)regexNode->isNegativeClass, (int)regexNode->numIntervals, (int)regexNode->minMatches, (int)regexNode->maxMatches, (int)regexNode->numMatches, (int)regexNode->isGreedy, (int)regexNode->hasLengthSpecified
		);

		for (int i = 0; i < regexNode->numIntervals; ++i) {
			interval *rangeInterval = &regexNode->characterRangeIntervals[i];
			printf("[%c, %c] (= [%d, %d])\n", (char)rangeInterval->min, (char)rangeInterval->max, rangeInterval->min, rangeInterval->max);
		}
	}

	if (regexNode->type == RegexType_MetaChar) {
		printf(
			"REGEX NODE: {type: %d, comparisonChar: %c, minMatches: %d, maxMatches: %d, numMatches: %d, isGreedy: %d, hasLengthSpecified: %d} \n",
			(int)regexNode->type, regexNode->comparisonChar, (int)regexNode->minMatches, (int)regexNode->maxMatches, (int)regexNode->numMatches, (int)regexNode->isGreedy, (int)regexNode->hasLengthSpecified
		);
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
	uint8_t parseLengthState = ParseCustomLengthState_NotStarted;
	int currentIndex = openingBracketIndex;
	bool continueLoop = true;
	bool wasMaxInitialized = false;

	while (continueLoop && (currentIndex < pattern->length || parseLengthState == ParseCustomLengthState_ClosingBracketGet)) {
		switch (parseLengthState) {
			case ParseCustomLengthState_NotStarted: {
				if (charAt(pattern, currentIndex) == '{') {
					parseLengthState = ParseCustomLengthState_OpeningBracketGet;
					++result.charsConsumed;
					++currentIndex;
				} else {
					result.hasError = true;
					printf("No opening bracket found. '%s' at %d\n", pattern->cstr, openingBracketIndex);
					return result;
				}
			} break;

			case ParseCustomLengthState_OpeningBracketGet: {
				int skipped = skipConsecutiveSpaces(pattern, currentIndex);
				currentIndex += skipped;
				result.charsConsumed += skipped;
				parseLengthState = ParseCustomLengthState_FirstNumStart;
			} break;

			case ParseCustomLengthState_FirstNumStart: {
				char currentChar = charAt(pattern, currentIndex);

				if (48 <= currentChar && currentChar <= 57) {
					result.minMatches = 10 * result.minMatches + ((uint8_t)currentChar - 48);
					++result.charsConsumed;
					++currentIndex;
				} else if (currentChar == ' ' || currentChar == ',') {
					parseLengthState = ParseCustomLengthState_FirstNumGet;
				} else {
					result.hasError = true;
					printf("Expected digit only, got: %c\n", currentChar);
					return result;
				}
			} break;

			case ParseCustomLengthState_FirstNumGet: {
				int skipped = skipConsecutiveSpaces(pattern, currentIndex);
				currentIndex += skipped;
				result.charsConsumed += skipped;
				char currentChar = charAt(pattern, currentIndex);

				if (currentChar == ',') {
					parseLengthState = ParseCustomLengthState_CommaGet;
					++currentIndex;
					++result.charsConsumed;
				} else {
					result.hasError = true;
					printf("Expected comma, got: %c\n", currentChar);
					return result;
				}
			} break;

			case ParseCustomLengthState_CommaGet: {
				int skipped = skipConsecutiveSpaces(pattern, currentIndex);
				currentIndex += skipped;
				result.charsConsumed += skipped;
				parseLengthState = ParseCustomLengthState_SecondNumStart;
			} break;

			case ParseCustomLengthState_SecondNumStart: {
				char currentChar = charAt(pattern, currentIndex);

				if (48 <= currentChar && currentChar <= 57) {
					wasMaxInitialized = true;
					result.maxMatches = 10 * result.maxMatches + ((uint8_t)currentChar - 48);
					++result.charsConsumed;
					++currentIndex;
				} else if (currentChar == ' ' || currentChar == '}') {
					parseLengthState = ParseCustomLengthState_SecondNumGet;
				} else {
					result.hasError = true;
					printf("Expected digit only, got: %c\n", currentChar);
					return result;
				}
			} break;

			case ParseCustomLengthState_SecondNumGet: {
				int skipped = skipConsecutiveSpaces(pattern, currentIndex);
				currentIndex += skipped;
				result.charsConsumed += skipped;
				char currentChar = charAt(pattern, currentIndex);

				if (currentChar == '}') {
					parseLengthState = ParseCustomLengthState_ClosingBracketGet;
					++currentIndex;
					++result.charsConsumed;
				} else {
					result.hasError = true;
					printf("Expected comma, got: %c\n", currentChar);
					return result;
				}
			} break;

			case ParseCustomLengthState_ClosingBracketGet: {
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

	if (parseLengthState != ParseCustomLengthState_ClosingBracketGet) {
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
	result->metaChars = PushArray(&GlobalArena, 256, char);

	bool shouldContinue = true;
	int currentIndex = index;
	uint8_t parsingState = ParseCharacterClassState_NotStarted;
	three_char_stack charsStack = {};

	while (shouldContinue && (currentIndex < pattern->length || parsingState == ParseCharacterClassState_ClosingBracketGet)) {
		// printf("state: %d. '%s', consumed: %d, currentChar: %c\n", (int)parsingState, patternRef, (int)result->charsConsumed, patternRef[0]);
		currentChar = charAt(pattern, currentIndex);

		switch (parsingState) {
			case ParseCharacterClassState_NotStarted: {
				if (currentChar != '[') {
					result->hasError = true;
					printf("No opening bracket ([) found\n");
					return resultWew;
				}
				++currentIndex;
				++result->charsConsumed;
				parsingState = ParseCharacterClassState_OpeningBracketGet;
			} break;

			case ParseCharacterClassState_OpeningBracketGet: {
				if (currentChar == '^') {
					result->isNegativeClass = true;
				} else { // -, ] are special if at the very beginning
					charsStack.hasLower = true;
					charsStack.lower = currentChar;
				}
				++currentIndex;
				++result->charsConsumed;
				parsingState = ParseCharacterClassState_NormalParsing;
			} break;

			case ParseCharacterClassState_NormalParsing: {
				if (currentChar == ']') {
					parsingState = ParseCharacterClassState_ClosingBracketGet;
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

			case ParseCharacterClassState_ClosingBracketGet: {
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

	if (parsingState != ParseCharacterClassState_ClosingBracketGet) {
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
		case CharContext_Regular: {
			escapableCharacters = "()|.*+?{[0\\";

			if (doesCharAtIndexMatchTestChar(pattern, index, '.')) {
				result.specialChar = '.';
				result.charType = SpecialChar_Meta;
				result.charsConsumed = 1;
				return result;
			}
		} break;

		case CharContext_CharacterClass: {
			escapableCharacters = "-]^0\\";
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
			result.charType = SpecialChar_Meta;
			result.charsConsumed = 2;
			return result;
		}
	}

	for (int i = 0; i < strLen(escapableCharacters); ++i) {
		if (doesCharAtIndexMatchTestChar(pattern, index + 1, escapableCharacters[i])) {
			result.specialChar = escapableCharacters[i];
			result.charType = SpecialChar_Literal;
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
				.type = RegexType_CharClass, 
				.isNegativeClass = characterClassResult.isNegativeClass,
				.numIntervals = characterClassResult.numIntervals,
				.characterRangeIntervals = characterClassResult.characterRangeIntervals,
				.minMatches = 1,
				.maxMatches = 1,
			};
			int onePastLengthQuantifier = i + characterClassResult.charsConsumed;
			i = onePastLengthQuantifier - 1; // ++i when the loop ends would skip a char otherwise
		} else {
			parse_special_character_result specialCharResult = parseSpecialCharacter(pattern, i, CharContext_Regular);

			if (specialCharResult.hasError) {
				return errorMachine;
			}

			if (specialCharResult.charsConsumed > 0) {
				if (specialCharResult.charType == SpecialChar_Literal) {
					node = regex_node{
						.type = RegexType_RegularChar, 
						.comparisonChar = specialCharResult.specialChar, 
						.minMatches = 1, 
						.maxMatches = 1
					};
				} else {
					node = regex_node{
						.type = RegexType_MetaChar, 
						.comparisonChar = specialCharResult.specialChar, 
						.minMatches = 1, 
						.maxMatches = 1
					};
				}
				int onePastLengthQuantifier = i + specialCharResult.charsConsumed;
				i = onePastLengthQuantifier - 1; // ++i when the loop ends would skip a char otherwise
			} else {
				node = regex_node{
					.type = RegexType_RegularChar, 
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

inline bool matchWildcard(char testChar)
{
	return testChar != '\n';
}

inline bool matchDigit(char testChar)
{
	return '0' <= testChar && testChar <= '9';
}

inline bool matchWord(char testChar)
{
	return ('a' <= testChar && testChar <= 'z') || ('A' <= testChar && testChar <= 'Z') || testChar == '_' || matchDigit(testChar);
}

inline bool matchSpace(char testChar)
{
	switch (testChar) {
		case ' ':
		case '\t':
		case '\n':
		case '\r':
		case '\v':
		case '\f':
			return true;
		default: return false;
	}
}

inline bool matchMetaChar(char metaChar, char testChar)
{
    switch (metaChar) {
        case '.': return matchWildcard(testChar);
        case 'd': return matchDigit(testChar);
        case 'D': return !matchDigit(testChar);
        case 's': return matchSpace(testChar);
        case 'S': return !matchSpace(testChar);
        case 'w': return matchWord(testChar);
        case 'W': return !matchWord(testChar);

        default:
            printf("unknown meta char: %c\n", metaChar);
            return false;
    }
}


match_result doesNodeMatch(regex_node *regexNode, char *testString) {
	match_result result = {};

	if (*testString == 0) { // todo: this should be binary-safe
		return result;
	}
	char currentChar = testString[0];

	if (regexNode->type == RegexType_RegularChar) {
		if (currentChar == regexNode->comparisonChar) {
			result.matched = true;
		}
	}

	if (regexNode->type == RegexType_CharClass) {
		bool matchedInterval = false;

		for (uint32_t i = 0; i < regexNode->numIntervals; ++i) {
			interval *characterRangeInterval = &regexNode->characterRangeIntervals[i];

			if (characterRangeInterval->min <= currentChar && currentChar <= characterRangeInterval->max) {
				matchedInterval = true;
			}
		}

		for (uint32_t i = 0; i < regexNode->numMetaChars && !matchedInterval; ++i) {
			char metaChar = regexNode->metaChars[i];
			matchedInterval = matchMetaChar(metaChar, currentChar);
		}
		// if it is negative, we want it to match 0 intervals. otherwise, even one match is good
		result.matched = regexNode->isNegativeClass ? !matchedInterval : matchedInterval;
	}

	if (regexNode->type == RegexType_MetaChar) {
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
	char pattern[] = ".*c";
	// char pattern[] = "\\w+";
	// char pattern[] = "\\d+";
	// char pattern[] = "\\{";
	// char pattern[] = "\\[a?b?]";
	// char pattern[] = "[ab\\]]+";
	// char pattern[] = "[[\\]a-z\\-]";
	char searchLines[][100] = {
		"abcde",
		"ab",
		"abcabcd",
		"aaaaabcaaabcaaaaaaaaaaaaab",
		"-]",
		"-]132[]11{[]",
		"[a-z]"
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