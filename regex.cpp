#include <iostream>

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
	TYPE_REGULAR_STRING = 1
};

struct regex_node
{
	uint8_t type;
	char *baseComparisonString;
};

struct match_result
{
	bool matched;
	uint32_t matchStart;
	uint32_t matchedLength;
};

struct regex_state_machine
{
	uint32_t numNodes;
	regex_node regexNodes[256];
};

regex_state_machine parseRegex(char *pattern)
{
	regex_state_machine stateMachine = {};
	int patternLength = strLen(pattern);
	int nodeBaseStringStart = 0;
	int nodeBaseStringEnd = 0;

	for (int i = 0; i < patternLength; ++i) {
		++nodeBaseStringEnd;
	}
	char* cmpStr = new char[nodeBaseStringEnd - nodeBaseStringStart];

	for (; nodeBaseStringStart < nodeBaseStringEnd; ++nodeBaseStringStart) {
		cmpStr[nodeBaseStringStart] = pattern[nodeBaseStringStart];
	}
	cmpStr[nodeBaseStringEnd] = 0;
	regex_node node = regex_node{RegexType::TYPE_REGULAR_STRING, cmpStr};
	stateMachine.regexNodes[stateMachine.numNodes++] = node;
	return stateMachine;
}

match_result doesNodeMatch(regex_node *regexNode, char *testString) {
	match_result result = {};

	if (regexNode->type == RegexType::TYPE_REGULAR_STRING) {
		int testStringLength = strLen(testString);
		int patternLength = strLen(regexNode->baseComparisonString);

		if (testStringLength < patternLength) {
			return result;
		}

		for (int j = 0; j <= (testStringLength - patternLength); ++j) {
			bool found = true;

			for (int k = 0; k < patternLength && found; ++k) {
				found = found && (regexNode->baseComparisonString[k] == testString[j + k]);
			}
			
			if (found) {
				result.matched = true;
				result.matchStart = j;
				result.matchedLength = patternLength;
				break;
			}
		}
	}
	return result;
}

void processString(char *testString, regex_state_machine *stateMachine)
{
	char *testStringRef = testString;
	int matchStart = 0;
	int matchEnd = 0;

	while (*testStringRef) {
		for (uint32_t i = 0; i < stateMachine->numNodes; ++i) {
			regex_node currentNode = stateMachine->regexNodes[i];
			match_result matchResult = doesNodeMatch(&currentNode, testStringRef);

			if (!matchResult.matched) {
				return;
			}
			// printf("PRE testString: %s, matchStart: %d, matchEnd: %d\n", testStringRef, matchStart, matchEnd);
			matchStart += matchResult.matchStart;
			matchEnd = matchStart + matchResult.matchedLength;
			testStringRef += matchEnd;
			// printf("POST testString: %s, matchStart: %d, matchEnd: %d\n", testStringRef, matchStart, matchEnd);
		}
		printf("matched: '%s' from %d to %d. Leftover string: '%s'\n", testString, matchStart, matchEnd, testStringRef);
		matchStart = matchEnd;
	}
}

int main(void)
{
	char pattern[] = "abc";
	char searchLines[][100] = {
		"abcde",
		"ab",
		"abcabcd",
		"aaaaabcaaabc"
	};
	int numLines = sizeof(searchLines) / sizeof(*searchLines);
	int patternLength = strLen(pattern);
	regex_state_machine stateMachine = parseRegex(pattern);

	for (int i = 0; i < numLines; ++i) {
		processString(searchLines[i], &stateMachine);
	}
	return 0;
}