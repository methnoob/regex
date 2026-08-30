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
	TYPE_REGULAR_CHAR_WITH_LENGTH = 1,
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
	uint32_t numNodes;
	regex_node regexNodes[256];
};

void addNodeToStateMachine(regex_node *regexNode, regex_state_machine *stateMachine) {
	// todo: assert?
	stateMachine->regexNodes[stateMachine->numNodes++] = *regexNode;
}

void printRegexNode(regex_node *regexNode) {
	printf(
		"REGEX NODE: {type: %d, comparisonChar: %c, minMatches: %d, maxMatches: %d, numMatches: %d} \n",
		(int)regexNode->type, regexNode->comparisonChar, (int)regexNode->minMatches, (int)regexNode->maxMatches, (int)regexNode->numMatches
	);
}

void printStateMachine(regex_state_machine *stateMachine) {
	printf("-----------------State Machine Start-----------------\n\n");

	for (uint32_t i = 0; i < stateMachine->numNodes; ++i) {
		printRegexNode(&stateMachine->regexNodes[i]);
	}
	printf("\n-----------------State Machine End-------------------\n");
}

regex_state_machine parseRegex(char *pattern)
{
	regex_state_machine stateMachine = {};
	int patternLength = strLen(pattern);
	
	for (int i = 0; i < patternLength; ++i) {
		regex_node node;

		if (i < patternLength - 1) {
			char nextChar = pattern[i + 1];
			bool consumedNextChar = false;
			// todo: '++' prevents backtracking. also, need to handle parsing {} for other min/max cases

			if (nextChar == '+') { 
				consumedNextChar = true;
				node = regex_node{RegexType::TYPE_REGULAR_CHAR_WITH_LENGTH, pattern[i], 1, 0};
			}

			if (nextChar == '?') {
				consumedNextChar = true;
				node = regex_node{RegexType::TYPE_REGULAR_CHAR_WITH_LENGTH, pattern[i], 0, 1};
			}
			// printf("current char: %c, next char: %c, consumed: %d\n", pattern[i], nextChar, (int)consumedNextChar);

			if (consumedNextChar) {
				++i;
				goto insertNode;
			}
		}
		node = regex_node{RegexType::TYPE_REGULAR_CHAR_WITH_LENGTH, pattern[i], 1, 1};
		insertNode: addNodeToStateMachine(&node, &stateMachine);
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
	return regexNode && (regexNode->numMatches > regexNode->minMatches);
}

void resetStateMachine(regex_state_machine *stateMachine) {
	for (uint32_t i = 0; i < stateMachine->numNodes; ++i) {
		stateMachine->regexNodes[i].numMatches = 0;
	}
}

void processString(char *testString, regex_state_machine *stateMachine)
{
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

			if (areLengthNodeMatchesMaxedOut(currentNode)) {
				// printf("node maxed out, moving ahead\n");
				continue; // move to the next node
			}

			if (areLengthNodeMatchesInRange(currentNode)) {
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
	char pattern[] = "a+bc?c";
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