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
	char comparisonChar;
};

struct match_result
{
	bool matched;
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
	
	for (int i = 0; i < patternLength; ++i) {
		regex_node node = regex_node{RegexType::TYPE_REGULAR_STRING, pattern[i]};
		stateMachine.regexNodes[stateMachine.numNodes++] = node;
	}
	return stateMachine;
}

match_result doesNodeMatch(regex_node *regexNode, char *testString) {
	match_result result = {};

	if (regexNode->type == RegexType::TYPE_REGULAR_STRING) {
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

void processString(char *testString, regex_state_machine *stateMachine)
{
	int matchStart = 0;
	int matchEnd = matchStart;
	char *testStringRef = testString + matchStart;

	while (*testStringRef) {
		bool matchedAllNodes = true;

		for (uint32_t i = 0; i < stateMachine->numNodes && matchedAllNodes; ++i) {
			regex_node currentNode = stateMachine->regexNodes[i];
			match_result matchResult = doesNodeMatch(&currentNode, testStringRef);

			if (!matchResult.matched) {
				matchedAllNodes = false;
				++matchStart; // move starting index ahead
				// reset ref and end index
				testStringRef = testString + matchStart;
				matchEnd = matchStart;
			} else {
				// move end index and ref ahead
				++matchEnd;
				++testStringRef;
			}
		}

		if (matchedAllNodes) {
			printf("matched: '%s' from %d to %d. Leftover string: '%s'\n", testString, matchStart, matchEnd, testStringRef);			
		}
		matchStart = matchEnd;
		matchedAllNodes = true;
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