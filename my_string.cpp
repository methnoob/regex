#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

// binary-safe string
struct my_string
{
	int length;
	char *cstr;
};

my_string fromCString(memory_arena *arena, char *cString, int length)
{
	if (length <= 0) {
		return my_string{};
	}
	my_string result = {};
	result.length = length;
	result.cstr = PushArray(arena, length + 1, char); // including the null terminator
	memcpy(result.cstr, cString, length + 1);
	return result;
}

my_string substring(my_string *original, int startIndex, int length)
{
	if (length <= 0 || startIndex >= original->length) {
		return my_string{};
	}
	int endIndex = startIndex + length - 1;
	endIndex = min(endIndex, original->length - 1);
	my_string result = {};
	result.length = endIndex - startIndex + 1;
	result.cstr = original->cstr + startIndex;
	return result;
}

char charAt(my_string *string, int index)
{
	// Assert(index < string->length);

	if (index >= string->length) {
		printf("index: %d exceeds length: %d\n", index, string->length);
		return 0;
	}
	return string->cstr[index];
}

bool doesCharAtIndexMatchTestChar(my_string *string, int index, char testChar)
{
	if (index >= string->length) {
		return false;
	}
	return charAt(string, index) == testChar;
}

int skipConsecutiveSpaces(my_string *string, int startIndex)
{
	int result = 0;

	while (doesCharAtIndexMatchTestChar(string, startIndex + result,  ' ')) {
		++result;
	}
	return result;
}