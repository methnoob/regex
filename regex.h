enum RegexType: uint8_t
{
	RegexType_RegularChar = 1,
	RegexType_CharClass = 2,
	RegexType_MetaChar = 3
};

enum SpecialCharType: uint8_t
{
	SpecialChar_Literal,
	SpecialChar_Meta
};

enum CharContext: uint8_t
{
	CharContext_Regular,
	CharContext_CharacterClass,
	CharContext_End
};

enum ParseCustomLengthState: uint8_t
{
	ParseCustomLengthState_NotStarted,
	ParseCustomLengthState_OpeningBracketGet,
	ParseCustomLengthState_FirstNumStart,
	ParseCustomLengthState_FirstNumGet,
	ParseCustomLengthState_CommaGet,
	ParseCustomLengthState_SecondNumStart,
	ParseCustomLengthState_SecondNumGet,
	ParseCustomLengthState_ClosingBracketGet
};

enum ParseCharacterClassState: uint8_t
{
	ParseCharacterClassState_NotStarted,
	ParseCharacterClassState_OpeningBracketGet,
	ParseCharacterClassState_NormalParsing,
	ParseCharacterClassState_ClosingBracketGet
};

struct parse_special_character_result
{
	bool hasError;
	char specialChar;
	SpecialCharType charType;
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
	RegexType type;

	// for a single char
	char comparisonChar;

	// character class
	bool isNegativeClass;
	uint32_t numIntervals;
	interval *characterRangeIntervals;

	uint32_t numMetaChars;
	char *metaChars;

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