enum RegexType
{
	TYPE_REGULAR_CHAR_WITH_LENGTH = 1,
	TYPE_CHAR_CLASS_WITH_LENGTH = 2,
	TYPE_META_CHAR_WITH_LENGTH = 3
};

enum SpecialChar
{
	SpecialChar_Literal,
	SpecialChar_Meta
};

enum CharContext
{
	CharContext_Regular,
	CharContext_CharacterClass,
	CharContext_End
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

struct parse_special_character_result
{
	bool hasError;
	char specialChar;
	SpecialChar charType;
	uint8_t charsConsumed;
};

struct parse_custom_length_result
{
	bool hasError;
	bool isGreedy;
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
	interval *characterRangeIntervals;

	uint32_t minMatches;
	uint32_t maxMatches;
	uint32_t numMatches;
	bool isGreedy;
	bool hasLengthSpecified;
};

struct match_result
{
	bool matched;
};

struct regex_state_machine
{
	my_string *originalPattern;
	bool hasError;
	uint32_t numNodes;
	regex_node *regexNodes;
};

struct parse_character_class_result
{
	bool hasError;
	uint32_t charsConsumed;
	bool isNegativeClass;
	uint32_t numIntervals;
	interval *characterRangeIntervals;
};

struct three_char_stack
{
	bool hasLower;
	char lower;
	bool hasRange;
	char upper;
};