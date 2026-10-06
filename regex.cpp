#include <stdio.h>
#include <cstdint>
#include "arena.cpp"
#include "my_string.cpp"
#include "regex.h"

static memory_arena GlobalArena;
static char metaCharacters[] = "dDsSwW";
static int numTabsForStateMachine;

void printStateMachine(regex_state_machine *stateMachine);
regex_state_machine *parseRegex(my_string *pattern, regex_parser_state *parserState, int startingIndex);


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
	char tabs[256];

	for (int i = 0; i < numTabsForStateMachine; ++i) {
		tabs[i] = '\t';
	}

	if (regexNode->type == RegexType_RegularChar) {
		printf(
			"%sREGEX NODE: {type: %d, comparisonChar: %c, minMatches: %d, maxMatches: %d, numMatches: %d, isGreedy: %d, hasLengthSpecified: %d} \n",
			tabs, (int)regexNode->type, regexNode->comparisonChar, (int)regexNode->minMatches, (int)regexNode->maxMatches, (int)regexNode->numMatches, (int)regexNode->isGreedy, (int)regexNode->hasLengthSpecified
		);
	}

	if (regexNode->type == RegexType_CharClass) {
		printf(
			"%sREGEX NODE: {type: %d, isNegativeClass: %d, numIntervals: %d, minMatches: %d, maxMatches: %d, numMatches: %d, isGreedy: %d, hasLengthSpecified: %d} \n",
			tabs, (int)regexNode->type, (int)regexNode->isNegativeClass, (int)regexNode->numIntervals, (int)regexNode->minMatches, (int)regexNode->maxMatches, (int)regexNode->numMatches, (int)regexNode->isGreedy, (int)regexNode->hasLengthSpecified
		);

		for (int i = 0; i < regexNode->numIntervals; ++i) {
			interval *rangeInterval = &regexNode->characterRangeIntervals[i];
			printf("%s[%c, %c] (= [%d, %d])\n", tabs, (char)rangeInterval->min, (char)rangeInterval->max, rangeInterval->min, rangeInterval->max);
		}

		for (int i = 0; i < regexNode->numMetaChars; ++i) {
			printf("%sMeta char: %c\n", tabs, regexNode->metaChars[i]);
		}
	}

	if (regexNode->type == RegexType_MetaChar) {
		printf(
			"%sREGEX NODE: {type: %d, comparisonChar: %c, minMatches: %d, maxMatches: %d, numMatches: %d, isGreedy: %d, hasLengthSpecified: %d} \n",
			tabs, (int)regexNode->type, regexNode->comparisonChar, (int)regexNode->minMatches, (int)regexNode->maxMatches, (int)regexNode->numMatches, (int)regexNode->isGreedy, (int)regexNode->hasLengthSpecified
		);
	}

	if (regexNode->type == RegexType_Group) {
		printf(
			"%sREGEX NODE: {type: %d, comparisonChar: %c, minMatches: %d, maxMatches: %d, numMatches: %d, isGreedy: %d, hasLengthSpecified: %d, numBranches: %d} \n",
			tabs, (int)regexNode->type, regexNode->comparisonChar, (int)regexNode->minMatches, (int)regexNode->maxMatches, (int)regexNode->numMatches, (int)regexNode->isGreedy, (int)regexNode->hasLengthSpecified, (int)regexNode->numBranches
		);

		for (int i = 0; i < regexNode->numBranches; ++i) {
			printf("%sbranch %d:\n", tabs, i);
			++numTabsForStateMachine;
			printStateMachine(regexNode->groupBranches + i);
			--numTabsForStateMachine;
		}
	}
}

void printStateMachine(regex_state_machine *stateMachine) {
	char tabs[256] = {};

	for (int i = 0; i < numTabsForStateMachine; ++i) {
		tabs[i] = '\t';
	}
	tabs[numTabsForStateMachine + 1] = 0;
	printf("%s-----------------State Machine Start-----------------\n\n", tabs);
	printf("%sOriginal pattern: %s\n\n", tabs, stateMachine->originalPattern->cstr);

	for (uint32_t i = 0; i < stateMachine->numNodes; ++i) {
		printRegexNode(&stateMachine->regexNodes[i]);
	}
	printf("\n%s-----------------State Machine End-------------------\n\n", tabs);
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

parse_token_result parseToken(my_string *pattern, int index, CharContext charContext)
{
	const char *escapableCharacters;
	parse_token_result result = {};

	switch (charContext) {
		case CharContext_Regular: {
			escapableCharacters = "()|.*+?{[0\\";

			if (doesCharAtIndexMatchTestChar(pattern, index, '.')) {
				result.token = '.';
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
		result.token = charAt(pattern, index);
		result.charType = SpecialChar_Literal;
		result.charsConsumed = 1;
		return result;
	}

	for (int i = 0; i < strLen(metaCharacters); ++i) {
		if (doesCharAtIndexMatchTestChar(pattern, index + 1, metaCharacters[i])) {
			result.token = metaCharacters[i];
			result.charType = SpecialChar_Meta;
			result.charsConsumed = 2;
			return result;
		}
	}

	for (int i = 0; i < strLen(escapableCharacters); ++i) {
		if (doesCharAtIndexMatchTestChar(pattern, index + 1, escapableCharacters[i])) {
			result.token = escapableCharacters[i];
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

bool addCharacterClassRange(parse_character_class_result *result, three_char_stack *stack)
{
	// printf("i am call. hasLower: %d, lower: %d, hasRange: %d, upper: %d\n", (int)stack->hasLower, (int)stack->lower, (int)stack->hasRange, (int)stack->upper);

	if (!stack->hasLower) {
		printf("Stack given when it has no lower item\n");
		return false;
	}
	// printf("LOWER: token: %c, specialType: %d, consumed: %d\n", stack->lower.token, stack->lower.charType, stack->lower.charsConsumed);

	if (!stack->hasRange) {
		if (stack->lower.charType == SpecialChar_Meta) {
			result->metaChars[result->numMetaChars++] = stack->lower.token;
			return true;
		}
		stack->hasRange = true;
		stack->upper = stack->lower;
	}

	if (stack->lower.charType == SpecialChar_Meta || stack->upper.charType == SpecialChar_Meta) {
		printf("meta chars cannot be part of a range\n");
		return false;
	}

	if (stack->upper.token < stack->lower.token) {
		printf("range end is smaller than range start\n");
		return false;
	}
	interval characterRangeInterval = {(uint8_t)stack->lower.token, (uint8_t)stack->upper.token};

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
		currentChar = charAt(pattern, currentIndex);
		parse_token_result currentTokenResult = parseToken(pattern, currentIndex, CharContext_CharacterClass);
		// printf("token: %c, specialType: %d, consumed: %d\n", currentTokenResult.token, currentTokenResult.charType, currentTokenResult.charsConsumed);

		if (currentTokenResult.hasError) {
			printf("error while parsing current token. Remaining string: '%s'\n", pattern->cstr + currentIndex);
			result->hasError = true;
			return resultWew;
		}

		switch (parsingState) {
			case ParseCharacterClassState_NotStarted: {
				if (currentChar != '[') {
					result->hasError = true;
					printf("No opening bracket ([) found\n");
					return resultWew;
				}
				++currentIndex;
				parsingState = ParseCharacterClassState_OpeningBracketGet;
			} break;

			case ParseCharacterClassState_OpeningBracketGet: {
				if (currentChar == '^') {
					result->isNegativeClass = true;
				} else { // -, ] are special if at the very beginning
					charsStack.hasLower = true;
					charsStack.lower = currentTokenResult;
				}
				currentIndex += currentTokenResult.charsConsumed;
				parsingState = ParseCharacterClassState_NormalParsing;
			} break;

			case ParseCharacterClassState_NormalParsing: {
				if (currentChar == ']') {
					parsingState = ParseCharacterClassState_ClosingBracketGet;
					++currentIndex;
					break;					
				}
				int nextIndex = currentIndex + currentTokenResult.charsConsumed;
				parse_token_result nextTokenResult = parseToken(pattern, nextIndex, CharContext_CharacterClass);

				if (nextTokenResult.hasError) {
					printf("error while parsing next token. Remaining string: '%s'\n", pattern->cstr + currentIndex);
					result->hasError = true;
					return resultWew;
				}
				// printf("current index: %d, next index: %d\n", currentIndex, nextIndex);
				// printf("token: %c, specialType: %d, consumed: %d\n", nextTokenResult.token, nextTokenResult.charType, nextTokenResult.charsConsumed);
				char nextChar = charAt(pattern, nextIndex);

				if (currentChar == '-' && nextChar && nextChar != ']') {
					if (!charsStack.hasLower) { // handles cases like [a-z-c], where the z is already in a previous range
						goto addCurrentChar;
					}
					charsStack.hasRange = true;
					charsStack.upper = nextTokenResult;

					currentIndex += 1 + nextTokenResult.charsConsumed;

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
				charsStack.lower = currentTokenResult;
				currentIndex += currentTokenResult.charsConsumed;
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
	result->charsConsumed = currentIndex - index;
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

parse_group_result parseGroup(my_string *pattern, int startingIndex, regex_parser_state *parserState)
{
	parse_group_result result = {};

	if (!doesCharAtIndexMatchTestChar(pattern, startingIndex, '(')) {
		return result;
	}
	pushState(parserState, RegexParserState_Group);
	int currentIndexInPattern = startingIndex + 1;
	result.groupBranches = PushArray(&GlobalArena, 256, regex_state_machine);

	while (true) {
		regex_state_machine *groupBranch = parseRegex(pattern, parserState, currentIndexInPattern);

		if (groupBranch->hasError) {
			result.hasError = true;
			printf("error while parsing group branch\n");
			break;
		}
		char currentChar = charAt(pattern, parserState->currentIndex);

		if (currentChar == '|' || currentChar == ')') {
			result.groupBranches[result.numBranches++] = *groupBranch;
			currentIndexInPattern = parserState->currentIndex + 1;

			if (currentChar == ')') {
				break;
			}
		} else {
			result.hasError = true;
			printf("string finished without ending the group\n");
			break;
		}
	}
	popState(parserState);
	result.charsConsumed = currentIndexInPattern - startingIndex;
	return result;
}

regex_state_machine *parseRegex(my_string *pattern, regex_parser_state *parserState, int startingIndex)
{
	regex_state_machine *errorMachine = PushStruct(&GlobalArena, regex_state_machine);
	errorMachine->hasError = true;

	regex_state_machine *stateMachine = PushStruct(&GlobalArena, regex_state_machine);
	stateMachine->originalPattern = pattern;
	stateMachine->regexNodes = PushArray(&GlobalArena, 256, regex_node);


	for (int i = startingIndex; i < pattern->length;) {
		parserState->currentIndex = i;
		regex_node node;
		regex_node *previousNode = stateMachine->numNodes == 0 ? NULL : &stateMachine->regexNodes[stateMachine->numNodes - 1];

		char currentChar = charAt(pattern, i);

		if (getCurrentParserState(parserState) == RegexParserState_Group) {
			if (currentChar == '|' || currentChar == ')') {
				if (i == startingIndex) {
					printf("empty group case\n");
					return errorMachine;
				}
				return stateMachine;
			}
		}

		parse_group_result groupResult = parseGroup(pattern, i, parserState);

		if (groupResult.hasError) {
			return errorMachine;
		}

		if (groupResult.charsConsumed > 0) {
			node = regex_node{
				.type = RegexType_Group, 
				.numBranches = groupResult.numBranches,
				.groupBranches = groupResult.groupBranches,
				.minMatches = 1,
				.maxMatches = 1,
			};
			addNodeToStateMachine(&node, stateMachine);
			i += groupResult.charsConsumed;
			continue;
		}

		parse_custom_length_result lengthResult = parseLengthQuantifier(pattern, i, stateMachine);

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

			i += lengthResult.charsConsumed;
			continue;
		}
		parse_character_class_result characterClassResult = parseCharacterClass(pattern, i, stateMachine);

		if (characterClassResult.hasError) {
			return errorMachine;
		}

		if (characterClassResult.charsConsumed > 0) {
			node = regex_node{
				.type = RegexType_CharClass, 
				.isNegativeClass = characterClassResult.isNegativeClass,
				.numIntervals = characterClassResult.numIntervals,
				.characterRangeIntervals = characterClassResult.characterRangeIntervals,
				.numMetaChars = characterClassResult.numMetaChars,
				.metaChars = characterClassResult.metaChars,
				.minMatches = 1,
				.maxMatches = 1,
			};
			i += characterClassResult.charsConsumed;
		} else {			
			parse_token_result tokenResult = parseToken(pattern, i, CharContext_Regular);

			if (tokenResult.hasError) {
				return errorMachine;
			}

			if (tokenResult.charsConsumed == 0) {
				printf("should always match a literal or escaped char\n");
				return errorMachine;
			}

			if (tokenResult.charType == SpecialChar_Literal) {
				node = regex_node{
					.type = RegexType_RegularChar, 
					.comparisonChar = tokenResult.token, 
					.minMatches = 1, 
					.maxMatches = 1
				};
			} else {
				node = regex_node{
					.type = RegexType_MetaChar, 
					.comparisonChar = tokenResult.token, 
					.minMatches = 1, 
					.maxMatches = 1
				};
			}
			i += tokenResult.charsConsumed;
		}
		addNodeToStateMachine(&node, stateMachine);
	}
	printStateMachine(stateMachine);
	return stateMachine;
}

regex_state_machine *parseRegex(my_string *pattern, regex_parser_state *parserState)
{
	return parseRegex(pattern, parserState, 0);
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
	bool matched = false;

	if (*testString == 0) { // todo: this should be binary-safe
		return result;
	}
	char currentChar = testString[0];

	if (regexNode->type == RegexType_RegularChar) {
		if (currentChar == regexNode->comparisonChar) {
			matched = true;
		}
	}

	if (regexNode->type == RegexType_CharClass) {
		bool matchedInterval = false;

		for (uint32_t i = 0; i < regexNode->numIntervals && !matchedInterval; ++i) {
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
		matched = regexNode->isNegativeClass ? !matchedInterval : matchedInterval;
	}

	if (regexNode->type == RegexType_MetaChar) {
		matched = matchMetaChar(regexNode->comparisonChar, testString[0]);
	}

	if (matched) {
		++result.numCharsMatched;
	}

	if (regexNode->type == RegexType_Group) {
		printf("groups support incoming\n");
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

bool nodeMatches(regex_node *regexNode, state_machine_partial *currentUniverseMatch, my_string *testString)
{
	if (currentUniverseMatch->matchEnd >= testString->length) {
		return false;
	}
	match_result match = doesNodeMatch(regexNode, testString->cstr + currentUniverseMatch->matchEnd);
	return match.numCharsMatched > 0;
}

void printStateMachinePartial(state_machine_partial *currentUniverseMatch)
{
	printf(
		"{matchStart: %d, matchEnd: %d, numMatchesForCurrentNode: %d}\n", 
		(int)currentUniverseMatch->matchStart,
		(int)currentUniverseMatch->matchEnd,
		(int)currentUniverseMatch->numMatchesForCurrentNode
	);
}

bool areMatchesInRange(regex_node *regexNode, int numMatches)
{
	return regexNode->minMatches <= numMatches && ((regexNode->maxMatches == 0) || numMatches <= regexNode->maxMatches);
}

state_machine_match matchStringToStateMachine(regex_state_machine *stateMachine, uint32_t startingNode, my_string *testString, int matchStart)
{
	// todo: handle greedy nodes (e.g. a*+)
	if (startingNode == stateMachine->numNodes) {
		// printf("hehehehe\n");
		return state_machine_match{true, matchStart, matchStart};
	}
	regex_node *currentNode = stateMachine->regexNodes + startingNode;
	// printf("testing match of '%s' from %d against node: \n", testString->cstr, matchStart);
	// printRegexNode(currentNode);
	// for super large test strings / pathological regexes, this should be a dynamic arr?
	// but we don't care about that right now
	size_t arenaOffset = GlobalArena.currentOffset;
	size_t maxAlternateStates = testString->length + 1; // for nodes that can match 0 times
	state_machine_partial *alternateUniverses = PushArray(&GlobalArena, maxAlternateStates, state_machine_partial);
	int numValidStates = 0;


	state_machine_partial greedyPartial = {};
	greedyPartial.matchStart = matchStart;
	greedyPartial.matchEnd = matchStart;

	while (true) {
		if (currentNode->maxMatches > 0 && greedyPartial.numMatchesForCurrentNode == currentNode->maxMatches) {
			break;
		}

		if (nodeMatches(currentNode, &greedyPartial, testString)) {
			++greedyPartial.matchEnd;
			++greedyPartial.numMatchesForCurrentNode;
			continue;
		}
		break;
	}
	// printf("num matches: %d\n", greedyPartial.numMatchesForCurrentNode);
	int minMatches = currentNode->minMatches;
	int maxMatches = (currentNode->maxMatches > 0) ? min(greedyPartial.numMatchesForCurrentNode, currentNode->maxMatches) : greedyPartial.numMatchesForCurrentNode;

	if (currentNode->isGreedy) {
		minMatches = maxMatches;
	}

	// emulate backtracking by going from the greediest match to the least greedy one
	for (int i = maxMatches; i >= minMatches; --i) {
		// printf("here. i: %d\n", i);
		state_machine_partial partial = {};
		partial.matchStart = matchStart;
		partial.matchEnd = matchStart + i;
		partial.numMatchesForCurrentNode = 0;

		if (areMatchesInRange(currentNode, i)) {
			// printf("adding partial\n");
			alternateUniverses[numValidStates++] = partial;
		}
	}

	if (numValidStates == 0) {
		GlobalArena.currentOffset = arenaOffset; // give back temp memory
		return state_machine_match{};
	}
	state_machine_match noMatch = {};
	state_machine_match *finalMatch = &noMatch;

	for (int i = 0; i < numValidStates; ++i) {
		state_machine_partial *currentPartial = alternateUniverses + i;
		state_machine_match match = matchStringToStateMachine(stateMachine, startingNode + 1, testString, currentPartial->matchEnd);

		if (match.matched) {
			finalMatch->matchStart = matchStart;
			finalMatch->matchEnd = match.matchEnd;
			finalMatch->matched = true;
			break; // greediest / first match preferred
		}
	}
	state_machine_match result = {};

	if (finalMatch->matched) {
		result = state_machine_match{true, finalMatch->matchStart, finalMatch->matchEnd};
	}
	GlobalArena.currentOffset = arenaOffset; // give back temp memory
	return result;
}

state_machine_match processString2(my_string *testString, int matchStart, regex_state_machine *stateMachine)
{
	state_machine_match result = matchStringToStateMachine(stateMachine, 0, testString, matchStart);

	if (result.matched) {
		printf("matched: '%s' from %d to %d. Leftover string: '%s'\n", testString->cstr, result.matchStart, result.matchEnd, testString->cstr + result.matchEnd);
	}
	return result;
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
		regex_node *previousNode = 0;

		for (uint32_t i = 0; i < stateMachine->numNodes && matchedAllNodes; ++i) {
			regex_node *currentNode = &stateMachine->regexNodes[i];
			match_result matchResult = doesNodeMatch(currentNode, testStringRef);
			// printRegexNode(currentNode);
			// printRegexNode(previousNode);

			if (matchResult.numCharsMatched == 0) {
				if (areLengthNodeMatchesInRange(currentNode)) {
					if (currentNode->numMatches > 0) {
						previousNode = currentNode; // if a node matches with 0, we don't need to backtrack by 1
					}
					continue; // move to the next node
				} else if (canNodeBeBacktracked(previousNode)) {
					// printf("backtracking:\n");
					// printf("back tracking node: \n");
					// printRegexNode(previousNode);
					// printRegexNode(currentNode);
					// trying to backtrack the previous node
					--matchEnd;
					--testStringRef;
					--previousNode->numMatches;
					--i;

					// continue the backtracking to the previous node
					if (previousNode->numMatches == 0 && previousNode != stateMachine->regexNodes) {
						--previousNode;
					}
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
				previousNode = currentNode;
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
	uint32_t bufferSize = (uint32_t)Megabytes(2000);
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
	// char pattern[] = "\\{";
	// char pattern[] = "\\[a?b?]";
	// char pattern[] = "[ab\\]]+";
	// char pattern[] = "[[\\]a-z\\-]+";
	// char pattern[] = "[\\^\\d]+";
	// char pattern[] = "[\\^\\D]+";
	// char pattern[] = "\\.*c";
	char pattern[] = "a*[a-z]{0,100}a";
	// char pattern[] = ".*.*";
	// char pattern[] = "ab(cd|(gg|[a-z]+)){1, 100}";
	char searchLines[][100] = {
		"abcde",
		"ab",
		"abcabcd",
		"aaaaabcaaabcaaaaaaaaaaaaab",
		"-]",
		"-]132[]11{[]",
		"[a-z]^",
		"...c",
		"a",
		// "a\nb"
	};
	int numLines = sizeof(searchLines) / sizeof(*searchLines);

	my_string patternString = fromCString(&GlobalArena, pattern, strLen(pattern));
	regex_parser_state *parserState = PushStruct(&GlobalArena, regex_parser_state);
	pushState(parserState, RegexParserState_Normal);

	regex_state_machine *stateMachine = parseRegex(&patternString, parserState);

	if (parserState->stackLen > 1) {
		printf("ERROR: parser ended in state: %d\n", (int)parserState->stateStack[parserState->stackLen - 1]);
	}

	for (int i = 0; i < numLines; ++i) {
		my_string testString = fromCString(&GlobalArena, searchLines[i], strLen(searchLines[i]));

		for (int j = 0; j < testString.length;) {
			state_machine_match match = processString2(&testString, j, stateMachine);
			// printf("string: '%s', j: %d, matched: %d, matchStart: %d, matchEnd: %d\n", testString.cstr, j, (int)match.matched, match.matchStart, match.matchEnd);

			if (!match.matched || match.matchEnd == j) {
				++j;
			} else {
				j = match.matchEnd;
			}
		}
		// processString(&testString, stateMachine);
	}
	printf("\n\nmemory used: %llu bytes\n\n", GlobalArena.currentOffset);
	return 0;
}