#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keyboard.h>
#include <algorithm>
#include <functional>
#include <chrono>
#include <string>
#include <regex>
#include <cmath>

#include "TextEditor.h"

#ifdef max
	#undef max
#endif
#ifdef min
	#undef min
#endif

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui/imgui.h> // for imGui::GetCurrentWindow()

// TODO
// - multiline comments vs single-line: latter is blocking start of a ML

template<class InputIt1, class InputIt2, class BinaryPredicate>
bool equals(InputIt1 first1, InputIt1 last1,
	InputIt2 first2, InputIt2 last2, BinaryPredicate p)
{
	for (; first1 != last1 && first2 != last2; ++first1, ++first2) 
	{
		if (!p(*first1, *first2))
			return false;
	}
	return first1 == last1 && first2 == last2;
}

TextEditor::TextEditor()
	: mLineSpacing(1.0f)
	, mUndoIndex(0)
	, mInsertSpaces(false)
	, mTabSize(4)
	, mAutocomplete(true)
	, mACOpened(false)
	, mHighlightLine(true)
	, mHorizontalScroll(true)
	, mCompleteBraces(true)
	, mShowLineNumbers(true)
	, mSmartIndent(true)
	, mOverwrite(false)
	, mReadOnly(false)
	, mWithinRender(false)
	, mScrollToCursor(false)
	, mScrollToTop(false)
	, mTextChanged(false)
	, mColorizerEnabled(true)
	, mTextStart(20.0f)
	, mLeftMargin(10)
	, mCursorPositionChanged(false)
	, mColorRangeMin(0)
	, mColorRangeMax(0)
	, mSelectionMode(SelectionMode::Normal)
	, mCheckComments(true)
	, mLastClick(-1.0f)
	, mHandleKeyboardInputs(true)
	, mHandleMouseInputs(true)
	, mIgnoreImGuiChild(false)
	, mShowWhitespaces(false)
	, mStartTime(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
{

	memset(mFindWord, 0, 256 * sizeof(char));
	mFindOpened = false;

	SetPalette(GetDarkPalette());
	SetLanguageDefinition(LanguageDefinition::HLSL());
	mLines.push_back(Line());

	m_shortcuts.resize((int)TextEditor::ShortcutID::Count);
	m_shortcuts[(int)TextEditor::ShortcutID::Undo] = TextEditor::Shortcut(SDLK_z, -1, 0, 1, 0); // CTRL+Z
	m_shortcuts[(int)TextEditor::ShortcutID::Redo] = TextEditor::Shortcut(SDLK_y, -1, 0, 1, 0); // CTRL+Y
	m_shortcuts[(int)TextEditor::ShortcutID::MoveUp] = TextEditor::Shortcut(SDLK_UP, -1, 0, 0, 2); // UP ARROW (+ SHIFT)
	m_shortcuts[(int)TextEditor::ShortcutID::MoveDown] = TextEditor::Shortcut(SDLK_DOWN, -1, 0, 0, 2); // DOWN ARROW (+ SHIFT)
	m_shortcuts[(int)TextEditor::ShortcutID::MoveLeft] = TextEditor::Shortcut(SDLK_LEFT, -1, 0, 2, 2); // LEFT ARROW (+ SHIFT/CTRL)
	m_shortcuts[(int)TextEditor::ShortcutID::MoveRight] = TextEditor::Shortcut(SDLK_RIGHT, -1, 0, 2, 2); // RIGHT ARROW (+ SHIFT/CTRL)
	m_shortcuts[(int)TextEditor::ShortcutID::MoveUpBlock] = TextEditor::Shortcut(SDLK_PAGEUP, -1, 0, 0, 2); // PAGE UP (+ SHIFT)
	m_shortcuts[(int)TextEditor::ShortcutID::MoveDownBlock] = TextEditor::Shortcut(SDLK_PAGEDOWN, -1, 0, 0, 2); // PAGE DOWN (+ SHIFT)
	m_shortcuts[(int)TextEditor::ShortcutID::MoveTop] = TextEditor::Shortcut(SDLK_HOME, -1, 1, 0, 2); // CTRL + HOME (+ SHIFT)
	m_shortcuts[(int)TextEditor::ShortcutID::MoveBottom] = TextEditor::Shortcut(SDLK_END, -1, 1, 0, 2); // CTRL + END (+ SHIFT)
	m_shortcuts[(int)TextEditor::ShortcutID::MoveStartLine] = TextEditor::Shortcut(SDLK_HOME, -1, 0, 0, 2); // HOME (+ SHIFT)
	m_shortcuts[(int)TextEditor::ShortcutID::MoveEndLine] = TextEditor::Shortcut(SDLK_END, -1, 0, 0, 2); // END (+ SHIFT)
	m_shortcuts[(int)TextEditor::ShortcutID::ForwardDelete] = TextEditor::Shortcut(SDLK_DELETE, -1, 0, 2, 0); // CTRL + DELETE
	m_shortcuts[(int)TextEditor::ShortcutID::BackwardDelete] = TextEditor::Shortcut(SDLK_BACKSPACE, -1, 0, 2, 0); // CTRL + BACKSPACE
	m_shortcuts[(int)TextEditor::ShortcutID::OverwriteCursor] = TextEditor::Shortcut(SDLK_INSERT, -1, 0, 0, 0); // INSERT
	m_shortcuts[(int)TextEditor::ShortcutID::Copy] = TextEditor::Shortcut(SDLK_c, -1, 0, 1, 0); // CTRL+C
	m_shortcuts[(int)TextEditor::ShortcutID::Paste] = TextEditor::Shortcut(SDLK_v, -1, 0, 1, 0); // CTRL+V
	m_shortcuts[(int)TextEditor::ShortcutID::Cut] = TextEditor::Shortcut(SDLK_x, -1, 0, 1, 0); // CTRL+X
	m_shortcuts[(int)TextEditor::ShortcutID::SelectAll] = TextEditor::Shortcut(SDLK_a, -1, 0, 1, 0); // CTRL+A
	m_shortcuts[(int)TextEditor::ShortcutID::AutocompleteOpen] = TextEditor::Shortcut(SDLK_SPACE, -1, 0, 1, 0); // CTRL+SPACE
	m_shortcuts[(int)TextEditor::ShortcutID::AutocompleteSelect] = TextEditor::Shortcut(SDLK_TAB, -1, 0, 0, 0); // TAB
	m_shortcuts[(int)TextEditor::ShortcutID::AutocompleteSelectActive] = TextEditor::Shortcut(SDLK_RETURN, -1, 0, 0, 0); // RETURN
	m_shortcuts[(int)TextEditor::ShortcutID::AutocompleteUp] = TextEditor::Shortcut(SDLK_UP, -1, 0, 0, 0); // UP ARROW
	m_shortcuts[(int)TextEditor::ShortcutID::AutocompleteDown] = TextEditor::Shortcut(SDLK_DOWN, -1, 0, 0, 2); // DOWN ARROW
	m_shortcuts[(int)TextEditor::ShortcutID::NewLine] = TextEditor::Shortcut(SDLK_RETURN, -1, 0, 0, 0); // RETURN
	m_shortcuts[(int)TextEditor::ShortcutID::IndentShift] = TextEditor::Shortcut(SDLK_TAB, -1, 0, 0, 2); // TAB
	m_shortcuts[(int)TextEditor::ShortcutID::Find] = TextEditor::Shortcut(SDLK_f, -1, 0, 0, 1); // CTRL+F
}

TextEditor::~TextEditor()
{
}

void TextEditor::SetLanguageDefinition(const LanguageDefinition & aLanguageDef)
{
	mLanguageDefinition = aLanguageDef;
	mRegexList.clear();

	for (auto& r : mLanguageDefinition.mTokenRegexStrings)
		mRegexList.push_back(std::make_pair(std::regex(r.first, std::regex_constants::optimize), r.second));

	Colorize();
}

void TextEditor::SetPalette(const Palette & aValue)
{
	mPaletteBase = aValue;
}

std::string TextEditor::GetText(const Coordinates & aStart, const Coordinates & aEnd) const
{
	std::string result;

	auto lstart = aStart.mLine;
	auto lend = aEnd.mLine;
	auto istart = GetCharacterIndex(aStart);
	auto iend = GetCharacterIndex(aEnd);
	size_t s = 0;

	for (size_t i = lstart; i < lend; i++)
		s += mLines[i].size();

	result.reserve(s + s / 8);

	while (istart < iend || lstart < lend)
	{
		if (lstart >= (int)mLines.size())
			break;

		auto& line = mLines[lstart];
		if (istart < (int)line.size())
		{
			result += line[istart].mChar;
			istart++;
		}
		else
		{
			istart = 0;
			++lstart;
			if (lstart < lend)
				result += '\n';
		}
	}

	return result;
}

TextEditor::Coordinates TextEditor::GetActualCursorCoordinates() const
{
	return SanitizeCoordinates(mState.mCursorPosition);
}

TextEditor::Coordinates TextEditor::SanitizeCoordinates(const Coordinates & aValue) const
{
	auto line = aValue.mLine;
	auto column = aValue.mColumn;
	if (line >= (int)mLines.size())
	{
		if (mLines.empty())
		{
			line = 0;
			column = 0;
		}
		else
		{
			line = (int)mLines.size() - 1;
			column = GetLineMaxColumn(line);
		}
		return Coordinates(line, column);
	}
	else
	{
		column = mLines.empty() ? 0 : std::min(column, GetLineMaxColumn(line));
		return Coordinates(line, column);
	}
}

// https://en.wikipedia.org/wiki/UTF-8
// We assume that the char is a standalone character (<128) or a leading byte of an UTF-8 code sequence (non-10xxxxxx code)
static int UTF8CharLength(TextEditor::Char c)
{
	if ((c & 0xFE) == 0xFC)
		return 6;
	if ((c & 0xFC) == 0xF8)
		return 5;
	if ((c & 0xF8) == 0xF0)
		return 4;
	else if ((c & 0xF0) == 0xE0)
		return 3;
	else if ((c & 0xE0) == 0xC0)
		return 2;
	return 1;
}

// "Borrowed" from ImGui source
static inline int ImTextCharToUtf8(char* buf, int buf_size, unsigned int c)
{
	if (c < 0x80)
	{
		buf[0] = (char)c;
		return 1;
	}
	if (c < 0x800)
	{
		if (buf_size < 2) return 0;
		buf[0] = (char)(0xc0 + (c >> 6));
		buf[1] = (char)(0x80 + (c & 0x3f));
		return 2;
	}
	if (c >= 0xdc00 && c < 0xe000)
	{
		return 0;
	}
	if (c >= 0xd800 && c < 0xdc00)
	{
		if (buf_size < 4) return 0;
		buf[0] = (char)(0xf0 + (c >> 18));
		buf[1] = (char)(0x80 + ((c >> 12) & 0x3f));
		buf[2] = (char)(0x80 + ((c >> 6) & 0x3f));
		buf[3] = (char)(0x80 + ((c) & 0x3f));
		return 4;
	}
	//else if (c < 0x10000)
	{
		if (buf_size < 3) return 0;
		buf[0] = (char)(0xe0 + (c >> 12));
		buf[1] = (char)(0x80 + ((c >> 6) & 0x3f));
		buf[2] = (char)(0x80 + ((c) & 0x3f));
		return 3;
	}
}

void TextEditor::Advance(Coordinates & aCoordinates) const
{
	if (aCoordinates.mLine < (int)mLines.size())
	{
		auto& line = mLines[aCoordinates.mLine];
		auto cindex = GetCharacterIndex(aCoordinates);

		if (cindex + 1 < (int)line.size())
		{
			auto delta = UTF8CharLength(line[cindex].mChar);
			cindex = std::min(cindex + delta, (int)line.size() - 1);
		}
		else
		{
			++aCoordinates.mLine;
			cindex = 0;
		}
		aCoordinates.mColumn = GetCharacterColumn(aCoordinates.mLine, cindex);
	}
}

void TextEditor::DeleteRange(const Coordinates & aStart, const Coordinates & aEnd)
{
	assert(aEnd >= aStart);
	assert(!mReadOnly);

	//printf("D(%d.%d)-(%d.%d)\n", aStart.mLine, aStart.mColumn, aEnd.mLine, aEnd.mColumn);

	if (aEnd == aStart)
		return;

	auto start = GetCharacterIndex(aStart);
	auto end = GetCharacterIndex(aEnd);

	if (aStart.mLine == aEnd.mLine)
	{
		auto& line = mLines[aStart.mLine];
		auto n = GetLineMaxColumn(aStart.mLine);
		if (aEnd.mColumn >= n)
			line.erase(line.begin() + start, line.end());
		else
			line.erase(line.begin() + start, line.begin() + end);
	}
	else
	{
		auto& firstLine = mLines[aStart.mLine];
		auto& lastLine = mLines[aEnd.mLine];

		firstLine.erase(firstLine.begin() + start, firstLine.end());
		lastLine.erase(lastLine.begin(), lastLine.begin() + end);

		if (aStart.mLine < aEnd.mLine)
			firstLine.insert(firstLine.end(), lastLine.begin(), lastLine.end());

		if (aStart.mLine < aEnd.mLine)
			RemoveLine(aStart.mLine + 1, aEnd.mLine + 1);
	}

	mTextChanged = true;
}

int TextEditor::InsertTextAt(Coordinates& /* inout */ aWhere, const char * aValue)
{
	assert(!mReadOnly);

	int cindex = GetCharacterIndex(aWhere);
	int totalLines = 0;
	while (*aValue != '\0')
	{
		assert(!mLines.empty());

		if (*aValue == '\r')
		{
			// skip
			++aValue;
		}
		else if (*aValue == '\n')
		{
			if (cindex < (int)mLines[aWhere.mLine].size())
			{
				auto& newLine = InsertLine(aWhere.mLine + 1);
				auto& line = mLines[aWhere.mLine];
				newLine.insert(newLine.begin(), line.begin() + cindex, line.end());
				line.erase(line.begin() + cindex, line.end());
			}
			else
			{
				InsertLine(aWhere.mLine + 1);
			}
			++aWhere.mLine;
			aWhere.mColumn = 0;
			cindex = 0;
			++totalLines;
			++aValue;
		}
		else
		{
			auto& line = mLines[aWhere.mLine];
			auto d = UTF8CharLength(*aValue);
			while (d-- > 0 && *aValue != '\0')
				line.insert(line.begin() + cindex++, Glyph(*aValue++, PaletteIndex::Default));
			++aWhere.mColumn;
		}

		mTextChanged = true;
	}

	return totalLines;
}

void TextEditor::AddUndo(UndoRecord& aValue)
{
	assert(!mReadOnly);
	//printf("AddUndo: (@%d.%d) +\'%s' [%d.%d .. %d.%d], -\'%s', [%d.%d .. %d.%d] (@%d.%d)\n",
	//	aValue.mBefore.mCursorPosition.mLine, aValue.mBefore.mCursorPosition.mColumn,
	//	aValue.mAdded.c_str(), aValue.mAddedStart.mLine, aValue.mAddedStart.mColumn, aValue.mAddedEnd.mLine, aValue.mAddedEnd.mColumn,
	//	aValue.mRemoved.c_str(), aValue.mRemovedStart.mLine, aValue.mRemovedStart.mColumn, aValue.mRemovedEnd.mLine, aValue.mRemovedEnd.mColumn,
	//	aValue.mAfter.mCursorPosition.mLine, aValue.mAfter.mCursorPosition.mColumn
	//	);

	mUndoBuffer.resize((size_t)(mUndoIndex + 1));
	mUndoBuffer.back() = aValue;
	++mUndoIndex;
}

TextEditor::Coordinates TextEditor::ScreenPosToCoordinates(const ImVec2& aPosition) const
{
	ImVec2 origin = ImGui::GetCursorScreenPos();
	ImVec2 local(aPosition.x - origin.x, aPosition.y - origin.y);

	int lineNo = std::max(0, (int)floor(local.y / mCharAdvance.y));

	int columnCoord = 0;

	if (lineNo >= 0 && lineNo < (int)mLines.size())
	{
		auto& line = mLines.at(lineNo);

		int columnIndex = 0;
		float columnX = 0.0f;

		while ((size_t)columnIndex < line.size())
		{
			float columnWidth = 0.0f;

			if (line[columnIndex].mChar == '\t')
			{
				float spaceSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, " ").x;
				float oldX = columnX;
				float newColumnX = (1.0f + std::floor((1.0f + columnX) / (float(mTabSize) * spaceSize))) * (float(mTabSize) * spaceSize);
				columnWidth = newColumnX - oldX;
				if (mTextStart + columnX + columnWidth * 0.5f > local.x)
					break;
				columnX = newColumnX;
				columnCoord = (columnCoord / mTabSize) * mTabSize + mTabSize;
				columnIndex++;
			}
			else
			{
				char buf[7];
				auto d = UTF8CharLength(line[columnIndex].mChar);
				int i = 0;
				while (i < 6 && d-- > 0)
					buf[i++] = line[columnIndex++].mChar;
				buf[i] = '\0';
				columnWidth = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf).x;
				if (mTextStart + columnX + columnWidth * 0.5f > local.x)
					break;
				columnX += columnWidth;
				columnCoord++;
			}
		}
	}

	return SanitizeCoordinates(Coordinates(lineNo, columnCoord));
}

TextEditor::Coordinates TextEditor::FindWordStart(const Coordinates & aFrom) const
{
	Coordinates at = aFrom;
	if (at.mLine >= (int)mLines.size())
		return at;

	auto& line = mLines[at.mLine];
	auto cindex = GetCharacterIndex(at);

	if (cindex >= (int)line.size())
		return at;

	while (cindex > 0 && isspace(line[cindex].mChar))
		--cindex;

	auto cstart = (PaletteIndex)line[cindex].mColorIndex;
	while (cindex > 0)
	{
		auto c = line[cindex].mChar;
		if ((c & 0xC0) != 0x80)	// not UTF code sequence 10xxxxxx
		{
			if (c <= 32 && isspace(c))
			{
				cindex++;
				break;
			}
			if (cstart != (PaletteIndex)line[size_t(cindex - 1)].mColorIndex)
				break;
		}
		--cindex;
	}
	return Coordinates(at.mLine, GetCharacterColumn(at.mLine, cindex));
}

TextEditor::Coordinates TextEditor::FindWordEnd(const Coordinates & aFrom) const
{
	Coordinates at = aFrom;
	if (at.mLine >= (int)mLines.size())
		return at;

	auto& line = mLines[at.mLine];
	auto cindex = GetCharacterIndex(at);

	if (cindex >= (int)line.size())
		return at;

	bool prevspace = (bool)isspace(line[cindex].mChar);
	auto cstart = (PaletteIndex)line[cindex].mColorIndex;
	while (cindex < (int)line.size())
	{
		auto c = line[cindex].mChar;
		auto d = UTF8CharLength(c);
		if (cstart != (PaletteIndex)line[cindex].mColorIndex)
			break;

		if (prevspace != !!isspace(c))
		{
			if (isspace(c))
				while (cindex < (int)line.size() && isspace(line[cindex].mChar))
					++cindex;
			break;
		}
		cindex += d;
	}
	return Coordinates(aFrom.mLine, GetCharacterColumn(aFrom.mLine, cindex));
}

TextEditor::Coordinates TextEditor::FindNextWord(const Coordinates & aFrom) const
{
	Coordinates at = aFrom;
	if (at.mLine >= (int)mLines.size())
		return at;

	// skip to the next non-word character
	auto cindex = GetCharacterIndex(aFrom);
	bool isword = false;
	bool skip = false;
	if (cindex < (int)mLines[at.mLine].size())
	{
		auto& line = mLines[at.mLine];
		isword = isalnum(line[cindex].mChar);
		skip = isword;
	}

	while (!isword || skip)
	{
		if (at.mLine >= mLines.size())
		{
			auto l = std::max(0, (int) mLines.size() - 1);
			return Coordinates(l, GetLineMaxColumn(l));
		}

		auto& line = mLines[at.mLine];
		if (cindex < (int)line.size())
		{
			isword = isalnum(line[cindex].mChar);

			if (isword && !skip)
				return Coordinates(at.mLine, GetCharacterColumn(at.mLine, cindex));

			if (!isword)
				skip = false;

			cindex++;
		}
		else
		{
			cindex = 0;
			++at.mLine;
			skip = false;
			isword = false;
		}
	}

	return at;
}

int TextEditor::GetCharacterIndex(const Coordinates& aCoordinates) const
{
	if (aCoordinates.mLine >= mLines.size())
		return -1;
	auto& line = mLines[aCoordinates.mLine];
	int c = 0;
	int i = 0;
	for (; i < line.size() && c < aCoordinates.mColumn;)
	{
		if (line[i].mChar == '\t')
			c = (c / mTabSize) * mTabSize + mTabSize;
		else
			++c;
		i += UTF8CharLength(line[i].mChar);
	}
	return i;
}

int TextEditor::GetCharacterColumn(int aLine, int aIndex) const
{
	if (aLine >= mLines.size())
		return 0;
	auto& line = mLines[aLine];
	int col = 0;
	int i = 0;
	while (i < aIndex && i < (int)line.size())
	{
		auto c = line[i].mChar;
		i += UTF8CharLength(c);
		if (c == '\t')
			col = (col / mTabSize) * mTabSize + mTabSize;
		else
			col++;
	}
	return col;
}

int TextEditor::GetLineCharacterCount(int aLine) const
{
	if (aLine >= mLines.size())
		return 0;
	auto& line = mLines[aLine];
	int c = 0;
	for (unsigned i = 0; i < line.size(); c++)
		i += UTF8CharLength(line[i].mChar);
	return c;
}

int TextEditor::GetLineMaxColumn(int aLine) const
{
	if (aLine >= mLines.size())
		return 0;
	auto& line = mLines[aLine];
	int col = 0;
	for (unsigned i = 0; i < line.size(); )
	{
		auto c = line[i].mChar;
		if (c == '\t')
			col = (col / mTabSize) * mTabSize + mTabSize;
		else
			col++;
		i += UTF8CharLength(c);
	}
	return col;
}

bool TextEditor::IsOnWordBoundary(const Coordinates & aAt) const
{
	if (aAt.mLine >= (int)mLines.size() || aAt.mColumn == 0)
		return true;

	auto& line = mLines[aAt.mLine];
	auto cindex = GetCharacterIndex(aAt);
	if (cindex >= (int)line.size())
		return true;

	if (mColorizerEnabled)
		return line[cindex].mColorIndex != line[size_t(cindex - 1)].mColorIndex;

	return isspace(line[cindex].mChar) != isspace(line[cindex - 1].mChar);
}

void TextEditor::RemoveLine(int aStart, int aEnd)
{
	assert(!mReadOnly);
	assert(aEnd >= aStart);
	assert(mLines.size() > (size_t)(aEnd - aStart));

	ErrorMarkers etmp;
	for (auto& i : mErrorMarkers)
	{
		ErrorMarkers::value_type e(i.first >= aStart ? i.first - 1 : i.first, i.second);
		if (e.first >= aStart && e.first <= aEnd)
			continue;
		etmp.insert(e);
	}
	mErrorMarkers = std::move(etmp);

	Breakpoints btmp;
	for (auto i : mBreakpoints)
	{
		if (i >= aStart && i <= aEnd)
			continue;
		btmp.insert(i >= aStart ? i - 1 : i);
	}
	mBreakpoints = std::move(btmp);

	mLines.erase(mLines.begin() + aStart, mLines.begin() + aEnd);
	assert(!mLines.empty());

	mTextChanged = true;
}

void TextEditor::RemoveLine(int aIndex)
{
	assert(!mReadOnly);
	assert(mLines.size() > 1);

	ErrorMarkers etmp;
	for (auto& i : mErrorMarkers)
	{
		ErrorMarkers::value_type e(i.first > aIndex ? i.first - 1 : i.first, i.second);
		if (e.first - 1 == aIndex)
			continue;
		etmp.insert(e);
	}
	mErrorMarkers = std::move(etmp);

	Breakpoints btmp;
	for (auto i : mBreakpoints)
	{
		if (i == aIndex)
			continue;
		btmp.insert(i >= aIndex ? i - 1 : i);
	}
	mBreakpoints = std::move(btmp);

	mLines.erase(mLines.begin() + aIndex);
	assert(!mLines.empty());

	mTextChanged = true;
}

TextEditor::Line& TextEditor::InsertLine(int aIndex)
{
	assert(!mReadOnly);

	auto& result = *mLines.insert(mLines.begin() + aIndex, Line());

	ErrorMarkers etmp;
	for (auto& i : mErrorMarkers)
		etmp.insert(ErrorMarkers::value_type(i.first >= aIndex ? i.first + 1 : i.first, i.second));
	mErrorMarkers = std::move(etmp);

	Breakpoints btmp;
	for (auto i : mBreakpoints)
		btmp.insert(i >= aIndex ? i + 1 : i);
	mBreakpoints = std::move(btmp);

	return result;
}

std::string TextEditor::GetWordUnderCursor() const
{
	auto c = GetCursorPosition();
	c.mColumn = std::max(c.mColumn - 1, 0);
	return GetWordAt(c);
}

std::string TextEditor::GetWordAt(const Coordinates & aCoords) const
{
	auto start = FindWordStart(aCoords);
	auto end = FindWordEnd(aCoords);

	std::string r;

	auto istart = GetCharacterIndex(start);
	auto iend = GetCharacterIndex(end);

	for (auto it = istart; it < iend; ++it)
		r.push_back(mLines[aCoords.mLine][it].mChar);

	return r;
}

ImU32 TextEditor::GetGlyphColor(const Glyph & aGlyph) const
{
	if (!mColorizerEnabled)
		return mPalette[(int)PaletteIndex::Default];
	if (aGlyph.mComment)
		return mPalette[(int)PaletteIndex::Comment];
	if (aGlyph.mMultiLineComment)
		return mPalette[(int)PaletteIndex::MultiLineComment];
	auto const color = mPalette[(int)aGlyph.mColorIndex];
	if (aGlyph.mPreprocessor)
	{
		const auto ppcolor = mPalette[(int)PaletteIndex::Preprocessor];
		const int c0 = ((ppcolor & 0xff) + (color & 0xff)) / 2;
		const int c1 = (((ppcolor >> 8) & 0xff) + ((color >> 8) & 0xff)) / 2;
		const int c2 = (((ppcolor >> 16) & 0xff) + ((color >> 16) & 0xff)) / 2;
		const int c3 = (((ppcolor >> 24) & 0xff) + ((color >> 24) & 0xff)) / 2;
		return ImU32(c0 | (c1 << 8) | (c2 << 16) | (c3 << 24));
	}

	return color;
}

void TextEditor::HandleKeyboardInputs()
{
	ImGuiIO& io = ImGui::GetIO();
	auto shift = io.KeyShift;
	auto ctrl = io.ConfigMacOSXBehaviors ? io.KeySuper : io.KeyCtrl;
	auto alt = io.ConfigMacOSXBehaviors ? io.KeyCtrl : io.KeyAlt;

	if (ImGui::IsWindowFocused())
	{
		if (ImGui::IsWindowHovered())
			ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);
		//ImGui::CaptureKeyboardFromApp(true);

		io.WantCaptureKeyboard = true;
		io.WantTextInput = true;

		ShortcutID actionID = ShortcutID::Count;
		for (int i = 0; i < m_shortcuts.size(); i++) {
			auto sct = m_shortcuts[i];

			if (sct.Key1 == -1)
				continue;
				
			ShortcutID curActionID = ShortcutID::Count;
			bool additionalChecks = true;

			SDL_Scancode sc1 = SDL_GetScancodeFromKey(sct.Key1);

			if (ImGui::IsKeyPressed(sc1) && ((sct.Key2 != -1 && ImGui::IsKeyPressed(SDL_GetScancodeFromKey(sct.Key2))) || sct.Key2 == -1)) {
				if (((sct.Ctrl == 0 && !ctrl) || (sct.Ctrl == 1 && ctrl) || (sct.Ctrl == 2)) &&		// ctrl check
					((sct.Alt == 0 && !alt) || (sct.Alt == 1 && alt) || (sct.Alt == 2)) &&			// alt check
					((sct.Shift == 0 && !shift) || (sct.Shift == 1 && shift) || (sct.Shift == 2))) {// shift check

					// PRESSED:
					curActionID = (TextEditor::ShortcutID)i;
					switch (curActionID) {
						case ShortcutID::Paste:
						case ShortcutID::Cut:
						case ShortcutID::Redo: 
						case ShortcutID::Undo:
						case ShortcutID::ForwardDelete:
						case ShortcutID::BackwardDelete:
							additionalChecks = !IsReadOnly();
						break;
						case ShortcutID::MoveUp:
						case ShortcutID::MoveDown:
							additionalChecks = !mACOpened;
						break;
						case ShortcutID::AutocompleteOpen: 
							additionalChecks = mAutocomplete;
						break;
						case ShortcutID::AutocompleteUp: 
						case ShortcutID::AutocompleteDown:
						case ShortcutID::AutocompleteSelect: 
							additionalChecks = mACOpened;
						break;
						case ShortcutID::AutocompleteSelectActive: 
							additionalChecks = mACOpened && mACSwitched;
						break;
						case ShortcutID::NewLine:
						case ShortcutID::IndentShift:
							additionalChecks = !IsReadOnly() && !mACOpened;
						break;
						default: break;
					}
				}
			}

			if (additionalChecks && curActionID != ShortcutID::Count)
				actionID = curActionID;
		}

		int keyCount = 0;
		bool keepACOpened = false;
		if (actionID != ShortcutID::Count) {
			switch (actionID) {
				case ShortcutID::Undo: Undo(); break;
				case ShortcutID::Redo: Redo(); break;
				case ShortcutID::MoveUp: MoveUp(1, shift); break;
				case ShortcutID::MoveDown: MoveDown(1, shift); break;
				case ShortcutID::MoveLeft: MoveLeft(1, shift, ctrl); break;
				case ShortcutID::MoveRight: MoveRight(1, shift, ctrl); break;
				case ShortcutID::MoveTop: MoveTop(shift); break;
				case ShortcutID::MoveBottom: MoveBottom(shift); break;
				case ShortcutID::MoveUpBlock: MoveUp(GetPageSize() - 4, shift); break;
				case ShortcutID::MoveDownBlock: MoveDown(GetPageSize() - 4, shift); break;
				case ShortcutID::MoveEndLine: MoveEnd(shift); break;
				case ShortcutID::MoveStartLine: MoveHome(shift); break;
				case ShortcutID::ForwardDelete:
					if (ctrl)
						MoveRight(1, true, true);
					Delete();
				break;
				case ShortcutID::BackwardDelete:
					if (ctrl)
						MoveLeft(1, true, true);
					Backspace();
				break;
				case ShortcutID::OverwriteCursor: mOverwrite ^= true; break;
				case ShortcutID::Copy: Copy(); break;
				case ShortcutID::Paste: Paste(); break;
				case ShortcutID::Cut: Cut(); break;
				case ShortcutID::SelectAll: SelectAll(); break;
				case ShortcutID::AutocompleteOpen: 
				{
					mACWord = GetWordUnderCursor();

					bool isValid = false;
					for (int i = 0; i < mACWord.size(); i++)
						if ((mACWord[i] >= 'a' && mACWord[i] <= 'z') || (mACWord[i] >= 'A' && mACWord[i] <= 'Z'))
						{
							isValid = true;
							break;
						}

					if (isValid) {
						mACSuggestions.clear();
						mACIndex = 0;
						mACSwitched = false;

						// starting with the written word
						for (auto& str : mLanguageDefinition.mKeywords)
							if (str.find(mACWord) == 0)
								mACSuggestions.push_back(str);

						for (auto& str : mLanguageDefinition.mIdentifiers)
							if (str.first.find(mACWord) == 0)
								mACSuggestions.push_back(str.first);

						// containing the word
						for (auto& str : mLanguageDefinition.mKeywords) {
							size_t ind = str.find(mACWord);
							if (ind > 0 && ind != std::string::npos)
								mACSuggestions.push_back(str);
						}

						for (auto& str : mLanguageDefinition.mIdentifiers) {
							size_t ind = str.first.find(mACWord);
							if (ind > 0 && ind != std::string::npos)
								mACSuggestions.push_back(str.first);
						}

						if (mACSuggestions.size() > 0) {
							mACOpened = true;
							keepACOpened = true;
							mACPosition = GetCursorPosition();
						}
					}
				} break;
				case ShortcutID::AutocompleteSelect: 
				case ShortcutID::AutocompleteSelectActive:
				{
					auto curCoord = GetCursorPosition();
					curCoord.mColumn = std::max<int>(curCoord.mColumn - 1, 0);
					
					auto acStart = FindWordStart(curCoord);
					auto acEnd = FindWordEnd(curCoord);
					
					SetSelection(acStart, acEnd);
					Backspace();
					InsertText(mACSuggestions[mACIndex]);
					
					mACOpened = false;
				}
				break;
				case ShortcutID::AutocompleteUp:
					mACIndex = std::max<int>(mACIndex - 1, 0), mACSwitched = true;
					keepACOpened = true;
				break;
				case ShortcutID::AutocompleteDown:
					mACIndex = std::min<int>(mACIndex + 1, (int)mACSuggestions.size()), mACSwitched = true;
					keepACOpened = true;
				break;
				case ShortcutID::NewLine:
					EnterCharacter('\n', false);
				break;
				case ShortcutID::IndentShift:
					EnterCharacter('\t', shift);
				break;
				case ShortcutID::Find: mFindOpened = true; break;
			}
		} else if (!IsReadOnly()) {
			for (int i = 0; i < io.InputQueueCharacters.Size; i++)
			{
				auto c = (unsigned char)io.InputQueueCharacters[i];
				if (c != 0 && (c == '\n' || c >= 32)) {
					EnterCharacter((char)c, shift);
					keyCount++;
				}
			}
			io.InputQueueCharacters.resize(0);
		}

		if (mACOpened) {
			for (size_t i = 0; i < ImGuiKey_COUNT; i++)
				keyCount += ImGui::IsKeyPressed(ImGui::GetKeyIndex(i));

			if (keyCount != 0 && !keepACOpened)
				mACOpened = false;
		}
	}
}

void TextEditor::HandleMouseInputs()
{
	ImGuiIO& io = ImGui::GetIO();
	auto shift = io.KeyShift;
	auto ctrl = io.ConfigMacOSXBehaviors ? io.KeySuper : io.KeyCtrl;
	auto alt = io.ConfigMacOSXBehaviors ? io.KeyCtrl : io.KeyAlt;

	if (ImGui::IsWindowHovered())
	{
		if (!shift && !alt)
		{
			auto click = ImGui::IsMouseClicked(0);
			auto doubleClick = ImGui::IsMouseDoubleClicked(0);
			auto t = ImGui::GetTime();
			auto tripleClick = click && !doubleClick && (mLastClick != -1.0f && (t - mLastClick) < io.MouseDoubleClickTime);

			/*
				Left mouse button triple click
			*/

			if (tripleClick)
			{
				if (!ctrl)
				{
					mState.mCursorPosition = mInteractiveStart = mInteractiveEnd = ScreenPosToCoordinates(ImGui::GetMousePos());
					mSelectionMode = SelectionMode::Line;
					SetSelection(mInteractiveStart, mInteractiveEnd, mSelectionMode);
				}

				mLastClick = -1.0f;
			}

			/*
				Left mouse button double click
			*/

			else if (doubleClick)
			{
				if (!ctrl)
				{
					mState.mCursorPosition = mInteractiveStart = mInteractiveEnd = ScreenPosToCoordinates(ImGui::GetMousePos());
					if (mSelectionMode == SelectionMode::Line)
						mSelectionMode = SelectionMode::Normal;
					else
						mSelectionMode = SelectionMode::Word;
					SetSelection(mInteractiveStart, mInteractiveEnd, mSelectionMode);
				}

				mLastClick = (float)ImGui::GetTime();
			}

			/*
				Left mouse button click
			*/
			else if (click)
			{
				mACOpened = false;
				mState.mCursorPosition = mInteractiveStart = mInteractiveEnd = ScreenPosToCoordinates(ImGui::GetMousePos());
				if (ctrl)
					mSelectionMode = SelectionMode::Word;
				else
					mSelectionMode = SelectionMode::Normal;
				SetSelection(mInteractiveStart, mInteractiveEnd, mSelectionMode);

				mLastClick = (float)ImGui::GetTime();
			}
			// Mouse left button dragging (=> update selection)
			else if (ImGui::IsMouseDragging(0) && ImGui::IsMouseDown(0))
			{
				io.WantCaptureMouse = true;
				mState.mCursorPosition = mInteractiveEnd = ScreenPosToCoordinates(ImGui::GetMousePos());
				SetSelection(mInteractiveStart, mInteractiveEnd, mSelectionMode);
			}
		}
	}
}

void TextEditor::RenderInternal(const char* aTitle)
{
	/* Compute mCharAdvance regarding to scaled font size (Ctrl + mouse wheel)*/
	const float fontSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, "#", nullptr, nullptr).x;
	mCharAdvance = ImVec2(fontSize, ImGui::GetTextLineHeightWithSpacing() * mLineSpacing);

	/* Update palette with the current alpha from style */
	for (int i = 0; i < (int)PaletteIndex::Max; ++i)
	{
		auto color = ImGui::ColorConvertU32ToFloat4(mPaletteBase[i]);
		color.w *= ImGui::GetStyle().Alpha;
		mPalette[i] = ImGui::ColorConvertFloat4ToU32(color);
	}

	assert(mLineBuffer.empty());
	mFocused = ImGui::IsWindowFocused();

	auto contentSize = ImGui::GetWindowContentRegionMax();
	auto drawList = ImGui::GetWindowDrawList();
	float longest(mTextStart);

	if (mScrollToTop)
	{
		mScrollToTop = false;
		ImGui::SetScrollY(0.f);
	}

	ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos();
	auto scrollX = ImGui::GetScrollX();
	auto scrollY = ImGui::GetScrollY();

	auto lineNo = (int)floor(scrollY / mCharAdvance.y);
	auto globalLineMax = (int)mLines.size();
	auto lineMax = std::max<int>(0, std::min<int>((int)mLines.size() - 1, lineNo + (int)floor((scrollY + contentSize.y) / mCharAdvance.y)));

	// Deduce mTextStart by evaluating mLines size (global lineMax) plus two spaces as text width
	char buf[16];
	snprintf(buf, 16, " %d ", globalLineMax);
	mTextStart = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf, nullptr, nullptr).x + mLeftMargin;

	if (!mLines.empty())
	{
		float spaceSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, " ", nullptr, nullptr).x;

		while (lineNo <= lineMax)
		{
			ImVec2 lineStartScreenPos = ImVec2(cursorScreenPos.x, cursorScreenPos.y + lineNo * mCharAdvance.y);
			ImVec2 textScreenPos = ImVec2(lineStartScreenPos.x + mTextStart, lineStartScreenPos.y);

			auto& line = mLines[lineNo];
			longest = std::max(mTextStart + TextDistanceToLineStart(Coordinates(lineNo, GetLineMaxColumn(lineNo))), longest);
			auto columnNo = 0;
			Coordinates lineStartCoord(lineNo, 0);
			Coordinates lineEndCoord(lineNo, GetLineMaxColumn(lineNo));

			// Draw selection for the current line
			float sstart = -1.0f;
			float ssend = -1.0f;

			assert(mState.mSelectionStart <= mState.mSelectionEnd);
			if (mState.mSelectionStart <= lineEndCoord)
				sstart = mState.mSelectionStart > lineStartCoord ? TextDistanceToLineStart(mState.mSelectionStart) : 0.0f;
			if (mState.mSelectionEnd > lineStartCoord)
				ssend = TextDistanceToLineStart(mState.mSelectionEnd < lineEndCoord ? mState.mSelectionEnd : lineEndCoord);

			if (mState.mSelectionEnd.mLine > lineNo)
				ssend += mCharAdvance.x;

			if (sstart != -1 && ssend != -1 && sstart < ssend)
			{
				ImVec2 vstart(lineStartScreenPos.x + mTextStart + sstart, lineStartScreenPos.y);
				ImVec2 vend(lineStartScreenPos.x + mTextStart + ssend, lineStartScreenPos.y + mCharAdvance.y);
				drawList->AddRectFilled(vstart, vend, mPalette[(int)PaletteIndex::Selection]);
			}

			// Draw breakpoints
			auto start = ImVec2(lineStartScreenPos.x + scrollX, lineStartScreenPos.y);

			if (mBreakpoints.count(lineNo + 1) != 0)
			{
				auto end = ImVec2(lineStartScreenPos.x + contentSize.x + 2.0f * scrollX, lineStartScreenPos.y + mCharAdvance.y);
				drawList->AddRectFilled(start, end, mPalette[(int)PaletteIndex::Breakpoint]);
			}

			// Draw error markers
			auto errorIt = mErrorMarkers.find(lineNo + 1);
			if (errorIt != mErrorMarkers.end())
			{
				auto end = ImVec2(lineStartScreenPos.x + contentSize.x + 2.0f * scrollX, lineStartScreenPos.y + mCharAdvance.y);
				drawList->AddRectFilled(start, end, mPalette[(int)PaletteIndex::ErrorMarker]);

				if (ImGui::IsMouseHoveringRect(lineStartScreenPos, end))
				{
					ImGui::BeginTooltip();
					ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(mPalette[(int)PaletteIndex::ErrorMessage]));
					ImGui::Text("Error at line %d:", errorIt->first);
					ImGui::PopStyleColor();
					ImGui::Separator();
					ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(mPalette[(int)PaletteIndex::ErrorMessage]));
					ImGui::Text("%s", errorIt->second.c_str());
					ImGui::PopStyleColor();
					ImGui::EndTooltip();
				}
			}

			// Draw line number (right aligned)
			if (mShowLineNumbers) {
				snprintf(buf, 16, "%d  ", lineNo + 1);

				auto lineNoWidth = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf, nullptr, nullptr).x;
				drawList->AddText(ImVec2(lineStartScreenPos.x + mTextStart - lineNoWidth, lineStartScreenPos.y), mPalette[(int)PaletteIndex::LineNumber], buf);
			}

			// Highlight the current line (where the cursor is)
			if (mState.mCursorPosition.mLine == lineNo)
			{
				auto focused = ImGui::IsWindowFocused();

				// Render the cursor
				if (focused)
				{
					auto timeEnd = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
					auto elapsed = timeEnd - mStartTime;
					if (elapsed > 400)
					{
						float width = 1.0f;
						auto cindex = GetCharacterIndex(mState.mCursorPosition);
						float cx = TextDistanceToLineStart(mState.mCursorPosition);

						if (mOverwrite && cindex < (int)line.size())
						{
							auto c = line[cindex].mChar;
							if (c == '\t')
							{
								auto x = (1.0f + std::floor((1.0f + cx) / (float(mTabSize) * spaceSize))) * (float(mTabSize) * spaceSize);
								width = x - cx;
							}
							else
							{
								char buf2[2];
								buf2[0] = line[cindex].mChar;
								buf2[1] = '\0';
								width = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf2).x;
							}
						}
						ImVec2 cstart(textScreenPos.x + cx, lineStartScreenPos.y);
						ImVec2 cend(textScreenPos.x + cx + width, lineStartScreenPos.y + mCharAdvance.y);
						drawList->AddRectFilled(cstart, cend, mPalette[(int)PaletteIndex::Cursor]);
						if (elapsed > 800)
							mStartTime = timeEnd;
					}
				}
			}

			// Render colorized text
			auto prevColor = line.empty() ? mPalette[(int)PaletteIndex::Default] : GetGlyphColor(line[0]);
			ImVec2 bufferOffset;

			for (int i = 0; i < line.size();)
			{
				auto& glyph = line[i];
				auto color = GetGlyphColor(glyph);

				if ((color != prevColor || glyph.mChar == '\t' || glyph.mChar == ' ') && !mLineBuffer.empty())
				{
					const ImVec2 newOffset(textScreenPos.x + bufferOffset.x, textScreenPos.y + bufferOffset.y);
					drawList->AddText(newOffset, prevColor, mLineBuffer.c_str());
					auto textSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, mLineBuffer.c_str(), nullptr, nullptr);
					bufferOffset.x += textSize.x;
					mLineBuffer.clear();
				}
				prevColor = color;

				if (glyph.mChar == '\t')
				{
					auto oldX = bufferOffset.x;
					bufferOffset.x = (1.0f + std::floor((1.0f + bufferOffset.x) / (float(mTabSize) * spaceSize))) * (float(mTabSize) * spaceSize);
					++i;

					if (mShowWhitespaces)
					{
						const auto s = ImGui::GetFontSize();
						const auto x1 = textScreenPos.x + oldX + 1.0f;
						const auto x2 = textScreenPos.x + bufferOffset.x - 1.0f;
						const auto y = textScreenPos.y + bufferOffset.y + s * 0.5f;
						const ImVec2 p1(x1, y);
						const ImVec2 p2(x2, y);
						const ImVec2 p3(x2 - s * 0.2f, y - s * 0.2f);
						const ImVec2 p4(x2 - s * 0.2f, y + s * 0.2f);
						drawList->AddLine(p1, p2, 0x90909090);
						drawList->AddLine(p2, p3, 0x90909090);
						drawList->AddLine(p2, p4, 0x90909090);
					}
				}
				else if (glyph.mChar == ' ')
				{
					if (mShowWhitespaces)
					{
						const auto s = ImGui::GetFontSize();
						const auto x = textScreenPos.x + bufferOffset.x + spaceSize * 0.5f;
						const auto y = textScreenPos.y + bufferOffset.y + s * 0.5f;
						drawList->AddCircleFilled(ImVec2(x, y), 1.5f, 0x80808080, 4);
					}
					bufferOffset.x += spaceSize;
					i++;
				}
				else
				{
					auto l = UTF8CharLength(glyph.mChar);
					while (l-- > 0)
						mLineBuffer.push_back(line[i++].mChar);
				}
				++columnNo;
			}

			if (!mLineBuffer.empty())
			{
				const ImVec2 newOffset(textScreenPos.x + bufferOffset.x, textScreenPos.y + bufferOffset.y);
				drawList->AddText(newOffset, prevColor, mLineBuffer.c_str());
				mLineBuffer.clear();
			}

			++lineNo;
		}

		// Draw a tooltip on known identifiers/preprocessor symbols
		if (ImGui::IsMousePosValid())
		{
			auto id = GetWordAt(ScreenPosToCoordinates(ImGui::GetMousePos()));
			if (!id.empty())
			{
				auto it = mLanguageDefinition.mIdentifiers.find(id);
				if (it != mLanguageDefinition.mIdentifiers.end())
				{
					ImGui::BeginTooltip();
					ImGui::TextUnformatted(it->second.mDeclaration.c_str());
					ImGui::EndTooltip();
				}
				else
				{
					auto pi = mLanguageDefinition.mPreprocIdentifiers.find(id);
					if (pi != mLanguageDefinition.mPreprocIdentifiers.end())
					{
						ImGui::BeginTooltip();
						ImGui::TextUnformatted(pi->second.mDeclaration.c_str());
						ImGui::EndTooltip();
					}
				}
			}
		}
	}
	
	// suggestions window
	if (mACOpened) {
		auto acCoord = mACPosition;
		acCoord.mColumn++;

		ImFont* font = ImGui::GetFont();
		ImGui::PopFont();

		ImGui::SetNextWindowPos(CoordinatesToScreenPos(acCoord), ImGuiCond_Always);
		ImGui::BeginChild("##texteditor_autocompl", ImVec2(150, 100), true);

		for (int i = 0; i < mACSuggestions.size(); i++) {
			ImGui::Selectable(mACSuggestions[i].c_str(), i == mACIndex);
			if (i == mACIndex)
				ImGui::SetScrollHereY();
		}

		ImGui::EndChild();

		ImGui::PushFont(font);

		ImGui::SetWindowFocus();
		if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape)))
			mACOpened = false;
	}

	ImGui::Dummy(ImVec2((longest + 2), mLines.size() * mCharAdvance.y));

	if (mScrollToCursor)
	{
		EnsureCursorVisible();
		ImGui::SetWindowFocus();
		mScrollToCursor = false;
	}
}

ImVec2 TextEditor::CoordinatesToScreenPos(const TextEditor::Coordinates& aPosition) const
{
	ImVec2 origin = ImGui::GetCursorScreenPos();
	int dist = 0;

	auto& line = mLines[aPosition.mLine];
	for (int i = 0; i < std::min(line.size(), (size_t)aPosition.mColumn); i++) {
		if (line[i].mChar == '\t')
			dist += mTabSize;
		else dist++;
	}

	int retY = origin.y + aPosition.mLine * mCharAdvance.y;
	int retX = origin.x + GetTextStart() * mCharAdvance.x + dist * mCharAdvance.x - ImGui::GetScrollX();

	return ImVec2(retX, retY);
}

void TextEditor::Render(const char* aTitle, const ImVec2& aSize, bool aBorder)
{
	mWithinRender = true;
	mCursorPositionChanged = false;

	ImVec2 findOrigin = ImGui::GetCursorScreenPos();
	float windowWidth = ImGui::GetWindowWidth();

	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertU32ToFloat4(mPalette[(int)PaletteIndex::Background]));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
	if (!mIgnoreImGuiChild)
		ImGui::BeginChild(aTitle, aSize, aBorder, (ImGuiWindowFlags_AlwaysHorizontalScrollbar * mHorizontalScroll) | ImGuiWindowFlags_NoMove);
	
	if (mHandleKeyboardInputs)
	{
		HandleKeyboardInputs();
		ImGui::PushAllowKeyboardFocus(true);
	}

	if (mHandleMouseInputs)
		HandleMouseInputs();

	ColorizeInternal();
	RenderInternal(aTitle);


	/* FIND TEXT WINDOW */
	if (mFindOpened)
	{
		ImFont* font = ImGui::GetFont();
		ImGui::PopFont();

		ImVec4 bgc = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];

		ImGui::SetNextWindowPos(ImVec2(findOrigin.x + windowWidth - 250, findOrigin.y), ImGuiCond_Always);
		ImGui::BeginChild(("##ted_findwnd" + std::string(aTitle)).c_str(), ImVec2(200, 40), true);
		
		ImGui::PushItemWidth(-15);
		if (ImGui::InputText(("##ted_findtextbox" + std::string(aTitle)).c_str(), mFindWord, 256, ImGuiInputTextFlags_EnterReturnsTrue)) {
			auto curPos = mState.mCursorPosition;
			size_t cindex = 0;
			for (size_t ln = 0; ln < curPos.mLine; ln++)
				cindex += GetLineCharacterCount(ln) + 1;
			cindex += curPos.mColumn;

			std::string textSrc = GetText();
			size_t textLoc = textSrc.find(mFindWord, cindex);
			if (textLoc == std::string::npos)
				textLoc = textSrc.find(mFindWord, 0);


			if (textLoc != std::string::npos) {
				curPos.mLine = curPos.mColumn = 0;
				cindex = 0;
				for (size_t ln = 0; ln < mLines.size(); ln++) {
					int charCount = GetLineCharacterCount(ln) + 1;
					if (cindex + charCount > textLoc) {
						curPos.mLine = ln;
						curPos.mColumn = textLoc - cindex;
						break;
					}
					else // just keep adding
						cindex += charCount;
				}


				auto selStart = curPos, selEnd = curPos;
				selEnd.mColumn += strlen(mFindWord);
				SetSelection(curPos, selEnd);
				SetCursorPosition(selEnd);

				ImGui::SetKeyboardFocusHere(0);
			}
		}
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (ImGui::Button(("X##" + std::string(aTitle)).c_str()))
			mFindOpened = false;

		ImGui::EndChild();

		ImGui::PushFont(font);

		if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape)))
			mFindOpened = false;
	}


	if (mHandleKeyboardInputs)
		ImGui::PopAllowKeyboardFocus();

	if (!mIgnoreImGuiChild)
		ImGui::EndChild();
	ImGui::PopStyleColor();
	ImGui::PopStyleVar();

	mWithinRender = false;
}

void TextEditor::SetText(const std::string & aText)
{
	mLines.clear();
	mLines.emplace_back(Line());
	for (auto chr : aText)
	{
		if (chr == '\r')
		{
			// ignore the carriage return character
		}
		else if (chr == '\n')
			mLines.emplace_back(Line());
		else
		{
			mLines.back().emplace_back(Glyph(chr, PaletteIndex::Default));
		}
	}

	mTextChanged = true;
	mScrollToTop = true;

	mUndoBuffer.clear();
	mUndoIndex = 0;

	Colorize();
}

void TextEditor::SetTextLines(const std::vector<std::string> & aLines)
{
	mLines.clear();

	if (aLines.empty())
	{
		mLines.emplace_back(Line());
	}
	else
	{
		mLines.resize(aLines.size());

		for (size_t i = 0; i < aLines.size(); ++i)
		{
			const std::string & aLine = aLines[i];

			mLines[i].reserve(aLine.size());
			for (size_t j = 0; j < aLine.size(); ++j)
				mLines[i].emplace_back(Glyph(aLine[j], PaletteIndex::Default));
		}
	}

	mTextChanged = true;
	mScrollToTop = true;

	mUndoBuffer.clear();
	mUndoIndex = 0;

	Colorize();
}

void TextEditor::EnterCharacter(ImWchar aChar, bool aShift)
{
	assert(!mReadOnly);

	UndoRecord u;

	u.mBefore = mState;

	if (HasSelection())
	{
		if (aChar == '\t' && mState.mSelectionStart.mLine != mState.mSelectionEnd.mLine)
		{
			auto start = mState.mSelectionStart;
			auto end = mState.mSelectionEnd;
			auto originalEnd = end;

			if (start > end)
				std::swap(start, end);
			start.mColumn = 0;
			//			end.mColumn = end.mLine < mLines.size() ? mLines[end.mLine].size() : 0;
			if (end.mColumn == 0 && end.mLine > 0)
				--end.mLine;
			if (end.mLine >= (int)mLines.size())
				end.mLine = mLines.empty() ? 0 : (int)mLines.size() - 1;
			end.mColumn = GetLineMaxColumn(end.mLine);

			//if (end.mColumn >= GetLineMaxColumn(end.mLine))
			//	end.mColumn = GetLineMaxColumn(end.mLine) - 1;

			u.mRemovedStart = start;
			u.mRemovedEnd = end;
			u.mRemoved = GetText(start, end);

			bool modified = false;

			for (int i = start.mLine; i <= end.mLine; i++)
			{
				auto& line = mLines[i];
				if (aShift)
				{
					if (!line.empty())
					{
						if (line.front().mChar == '\t')
						{
							line.erase(line.begin());
							modified = true;
						}
						else
						{
							for (int j = 0; j < mTabSize && !line.empty() && line.front().mChar == ' '; j++)
							{
								line.erase(line.begin());
								modified = true;
							}
						}
					}
				}
				else
				{
					if (mInsertSpaces) {
						for (int i = 0; i < mTabSize; i++)
							line.insert(line.begin(), Glyph(' ', TextEditor::PaletteIndex::Background));
					} else
						line.insert(line.begin(), Glyph('\t', TextEditor::PaletteIndex::Background));
					modified = true;
				}
			}

			if (modified)
			{
				start = Coordinates(start.mLine, GetCharacterColumn(start.mLine, 0));
				Coordinates rangeEnd;
				if (originalEnd.mColumn != 0)
				{
					end = Coordinates(end.mLine, GetLineMaxColumn(end.mLine));
					rangeEnd = end;
					u.mAdded = GetText(start, end);
				}
				else
				{
					end = Coordinates(originalEnd.mLine, 0);
					rangeEnd = Coordinates(end.mLine - 1, GetLineMaxColumn(end.mLine - 1));
					u.mAdded = GetText(start, rangeEnd);
				}

				u.mAddedStart = start;
				u.mAddedEnd = rangeEnd;
				u.mAfter = mState;

				mState.mSelectionStart = start;
				mState.mSelectionEnd = end;
				AddUndo(u);

				mTextChanged = true;

				EnsureCursorVisible();
			}

			return;
		}
		else
		{
			u.mRemoved = GetSelectedText();
			u.mRemovedStart = mState.mSelectionStart;
			u.mRemovedEnd = mState.mSelectionEnd;
			DeleteSelection();
		}
	}

	auto coord = GetActualCursorCoordinates();
	u.mAddedStart = coord;

	if (mLines.empty())
		mLines.push_back(Line());

	if (aChar == '\n')
	{
		InsertLine(coord.mLine + 1);
		auto& line = mLines[coord.mLine];
		auto& newLine = mLines[coord.mLine + 1];

		if (mLanguageDefinition.mAutoIndentation && mSmartIndent)
			for (size_t it = 0; it < line.size() && isascii(line[it].mChar) && isblank(line[it].mChar); ++it)
				newLine.push_back(line[it]);

		const size_t whitespaceSize = newLine.size();
		auto cindex = GetCharacterIndex(coord);
		newLine.insert(newLine.end(), line.begin() + cindex, line.end());
		line.erase(line.begin() + cindex, line.begin() + line.size());
		SetCursorPosition(Coordinates(coord.mLine + 1, GetCharacterColumn(coord.mLine + 1, (int)whitespaceSize)));
		u.mAdded = (char)aChar;
	}
	else
	{
		char buf[7];
		int e = ImTextCharToUtf8(buf, 7, aChar);
		if (e > 0)
		{
			if (mInsertSpaces && e == 1 && buf[0] == '\t') {
				for (int i = 0; i < mTabSize; i++)
					buf[i] = ' ';
				e = mTabSize;
			}
			buf[e] = '\0';

			auto& line = mLines[coord.mLine];
			auto cindex = GetCharacterIndex(coord);

			if (mOverwrite && cindex < (int)line.size())
			{
				auto d = UTF8CharLength(line[cindex].mChar);

				u.mRemovedStart = mState.mCursorPosition;
				u.mRemovedEnd = Coordinates(coord.mLine, GetCharacterColumn(coord.mLine, cindex + d));

				while (d-- > 0 && cindex < (int)line.size())
				{
					u.mRemoved += line[cindex].mChar;
					line.erase(line.begin() + cindex);
				}
			}

			for (auto p = buf; *p != '\0'; p++, ++cindex)
				line.insert(line.begin() + cindex, Glyph(*p, PaletteIndex::Default));
			u.mAdded = buf;

			SetCursorPosition(Coordinates(coord.mLine, GetCharacterColumn(coord.mLine, cindex)));
		}
		else
			return;
	}

	mTextChanged = true;

	u.mAddedEnd = GetActualCursorCoordinates();
	u.mAfter = mState;

	AddUndo(u);

	Colorize(coord.mLine - 1, 3);
	EnsureCursorVisible();

	// auto brace completion
	if (mCompleteBraces) {
		if (aChar == '{') {
			EnterCharacter('\n', false);
			EnterCharacter('}', false);
		} else if (aChar == '(')
			EnterCharacter(')', false);
		else if (aChar == '[')
			EnterCharacter(']', false);

		if (aChar == '{' || aChar == '(' || aChar == '[')
			mState.mCursorPosition.mColumn--;
	}
}

void TextEditor::SetReadOnly(bool aValue)
{
	mReadOnly = aValue;
}

void TextEditor::SetColorizerEnable(bool aValue)
{
	mColorizerEnabled = aValue;
}

void TextEditor::SetCursorPosition(const Coordinates & aPosition)
{
	if (mState.mCursorPosition != aPosition)
	{
		mState.mCursorPosition = aPosition;
		mCursorPositionChanged = true;
		EnsureCursorVisible();
	}
}

void TextEditor::SetSelectionStart(const Coordinates & aPosition)
{
	mState.mSelectionStart = SanitizeCoordinates(aPosition);
	if (mState.mSelectionStart > mState.mSelectionEnd)
		std::swap(mState.mSelectionStart, mState.mSelectionEnd);
}

void TextEditor::SetSelectionEnd(const Coordinates & aPosition)
{
	mState.mSelectionEnd = SanitizeCoordinates(aPosition);
	if (mState.mSelectionStart > mState.mSelectionEnd)
		std::swap(mState.mSelectionStart, mState.mSelectionEnd);
}

void TextEditor::SetSelection(const Coordinates & aStart, const Coordinates & aEnd, SelectionMode aMode)
{
	auto oldSelStart = mState.mSelectionStart;
	auto oldSelEnd = mState.mSelectionEnd;

	mState.mSelectionStart = SanitizeCoordinates(aStart);
	mState.mSelectionEnd = SanitizeCoordinates(aEnd);
	if (mState.mSelectionStart > mState.mSelectionEnd)
		std::swap(mState.mSelectionStart, mState.mSelectionEnd);

	switch (aMode)
	{
	case TextEditor::SelectionMode::Normal:
		break;
	case TextEditor::SelectionMode::Word:
	{
		mState.mSelectionStart = FindWordStart(mState.mSelectionStart);
		if (!IsOnWordBoundary(mState.mSelectionEnd))
			mState.mSelectionEnd = FindWordEnd(FindWordStart(mState.mSelectionEnd));
		break;
	}
	case TextEditor::SelectionMode::Line:
	{
		const auto lineNo = mState.mSelectionEnd.mLine;
		const auto lineSize = (size_t)lineNo < mLines.size() ? mLines[lineNo].size() : 0;
		mState.mSelectionStart = Coordinates(mState.mSelectionStart.mLine, 0);
		mState.mSelectionEnd = Coordinates(lineNo, GetLineMaxColumn(lineNo));
		break;
	}
	default:
		break;
	}

	if (mState.mSelectionStart != oldSelStart ||
		mState.mSelectionEnd != oldSelEnd)
		mCursorPositionChanged = true;
}

void TextEditor::InsertText(const std::string & aValue)
{
	InsertText(aValue.c_str());
}

void TextEditor::InsertText(const char * aValue)
{
	if (aValue == nullptr)
		return;

	auto pos = GetActualCursorCoordinates();
	auto start = std::min<Coordinates>(pos, mState.mSelectionStart);
	int totalLines = pos.mLine - start.mLine;

	totalLines += InsertTextAt(pos, aValue);

	SetSelection(pos, pos);
	SetCursorPosition(pos);
	Colorize(start.mLine - 1, totalLines + 2);
}

void TextEditor::DeleteSelection()
{
	assert(mState.mSelectionEnd >= mState.mSelectionStart);

	if (mState.mSelectionEnd == mState.mSelectionStart)
		return;

	DeleteRange(mState.mSelectionStart, mState.mSelectionEnd);

	SetSelection(mState.mSelectionStart, mState.mSelectionStart);
	SetCursorPosition(mState.mSelectionStart);
	Colorize(mState.mSelectionStart.mLine, 1);
}

void TextEditor::MoveUp(int aAmount, bool aSelect)
{
	auto oldPos = mState.mCursorPosition;
	mState.mCursorPosition.mLine = std::max<int>(0, mState.mCursorPosition.mLine - aAmount);
	if (oldPos != mState.mCursorPosition)
	{
		if (aSelect)
		{
			if (oldPos == mInteractiveStart)
				mInteractiveStart = mState.mCursorPosition;
			else if (oldPos == mInteractiveEnd)
				mInteractiveEnd = mState.mCursorPosition;
			else
			{
				mInteractiveStart = mState.mCursorPosition;
				mInteractiveEnd = oldPos;
			}
		}
		else
			mInteractiveStart = mInteractiveEnd = mState.mCursorPosition;
		SetSelection(mInteractiveStart, mInteractiveEnd);

		EnsureCursorVisible();
	}
}

void TextEditor::MoveDown(int aAmount, bool aSelect)
{
	assert(mState.mCursorPosition.mColumn >= 0);
	auto oldPos = mState.mCursorPosition;
	mState.mCursorPosition.mLine = std::max<int>(0, std::min<int>((int)mLines.size() - 1, mState.mCursorPosition.mLine + aAmount));

	if (mState.mCursorPosition != oldPos)
	{
		if (aSelect)
		{
			if (oldPos == mInteractiveEnd)
				mInteractiveEnd = mState.mCursorPosition;
			else if (oldPos == mInteractiveStart)
				mInteractiveStart = mState.mCursorPosition;
			else
			{
				mInteractiveStart = oldPos;
				mInteractiveEnd = mState.mCursorPosition;
			}
		}
		else
			mInteractiveStart = mInteractiveEnd = mState.mCursorPosition;
		SetSelection(mInteractiveStart, mInteractiveEnd);

		EnsureCursorVisible();
	}
}

static bool IsUTFSequence(char c)
{
	return (c & 0xC0) == 0x80;
}

void TextEditor::MoveLeft(int aAmount, bool aSelect, bool aWordMode)
{
	if (mLines.empty())
		return;

	auto oldPos = mState.mCursorPosition;
	mState.mCursorPosition = GetActualCursorCoordinates();
	auto line = mState.mCursorPosition.mLine;
	auto cindex = GetCharacterIndex(mState.mCursorPosition);

	while (aAmount-- > 0)
	{
		if (cindex == 0)
		{
			if (line > 0)
			{
				--line;
				if ((int)mLines.size() > line)
					cindex = (int)mLines[line].size();
				else
					cindex = 0;
			}
		}
		else
		{
			--cindex;
			if (cindex > 0)
			{
				if ((int)mLines.size() > line)
				{
					while (cindex > 0 && IsUTFSequence(mLines[line][cindex].mChar))
						--cindex;
				}
			}
		}

		mState.mCursorPosition = Coordinates(line, GetCharacterColumn(line, cindex));
		if (aWordMode)
		{
			mState.mCursorPosition = FindWordStart(mState.mCursorPosition);
			cindex = GetCharacterIndex(mState.mCursorPosition);
		}
	}

	mState.mCursorPosition = Coordinates(line, GetCharacterColumn(line, cindex));

	assert(mState.mCursorPosition.mColumn >= 0);
	if (aSelect)
	{
		mInteractiveStart = mState.mSelectionStart;
		mInteractiveEnd = mState.mSelectionEnd;

		if (oldPos == mInteractiveStart)
			mInteractiveStart = mState.mCursorPosition;
		else if (oldPos == mInteractiveEnd)
			mInteractiveEnd = mState.mCursorPosition;
		else
		{
			mInteractiveStart = mState.mCursorPosition;
			mInteractiveEnd = oldPos;
		}
	}
	else
		mInteractiveStart = mInteractiveEnd = mState.mCursorPosition;
	SetSelection(mInteractiveStart, mInteractiveEnd, SelectionMode::Normal);

	EnsureCursorVisible();
}

void TextEditor::MoveRight(int aAmount, bool aSelect, bool aWordMode)
{
	auto oldPos = mState.mCursorPosition;

	if (mLines.empty() || oldPos.mLine >= mLines.size())
		return;
		
	mState.mCursorPosition = GetActualCursorCoordinates();
	auto line = mState.mCursorPosition.mLine;
	auto cindex = GetCharacterIndex(mState.mCursorPosition);

	while (aAmount-- > 0)
	{
		auto lindex = mState.mCursorPosition.mLine;
		auto& line = mLines[lindex];

		if (cindex >= line.size())
		{
			if (mState.mCursorPosition.mLine < mLines.size() - 1)
			{
				mState.mCursorPosition.mLine = std::max(0, std::min((int)mLines.size() - 1, mState.mCursorPosition.mLine + 1));
				mState.mCursorPosition.mColumn = 0;
			}
			else
				return;
		}
		else
		{
			cindex += UTF8CharLength(line[cindex].mChar);
			mState.mCursorPosition = Coordinates(lindex, GetCharacterColumn(lindex, cindex));
			if (aWordMode)
				mState.mCursorPosition = FindWordEnd(mState.mCursorPosition);
		}
	}

	if (aSelect)
	{
		mInteractiveStart = mState.mSelectionStart;
		mInteractiveEnd = mState.mSelectionEnd;

		if (oldPos == mInteractiveEnd)
			mInteractiveEnd = mState.mCursorPosition;
		else if (oldPos == mInteractiveStart)
			mInteractiveStart = mState.mCursorPosition;
		else
		{
			mInteractiveStart = oldPos;
			mInteractiveEnd = mState.mCursorPosition;
		}
	}
	else
		mInteractiveStart = mInteractiveEnd = mState.mCursorPosition;
	SetSelection(mInteractiveStart, mInteractiveEnd, SelectionMode::Normal);

	EnsureCursorVisible();
}

void TextEditor::MoveTop(bool aSelect)
{
	auto oldPos = mState.mCursorPosition;
	SetCursorPosition(Coordinates(0, 0));

	if (mState.mCursorPosition != oldPos)
	{
		if (aSelect)
		{
			mInteractiveEnd = oldPos;
			mInteractiveStart = mState.mCursorPosition;
		}
		else
			mInteractiveStart = mInteractiveEnd = mState.mCursorPosition;
		SetSelection(mInteractiveStart, mInteractiveEnd);
	}
}

void TextEditor::TextEditor::MoveBottom(bool aSelect)
{
	auto oldPos = GetCursorPosition();
	auto newPos = Coordinates((int)mLines.size() - 1, 0);
	SetCursorPosition(newPos);
	if (aSelect)
	{
		mInteractiveStart = oldPos;
		mInteractiveEnd = newPos;
	}
	else
		mInteractiveStart = mInteractiveEnd = newPos;
	SetSelection(mInteractiveStart, mInteractiveEnd);
}

void TextEditor::MoveHome(bool aSelect)
{
	auto oldPos = mState.mCursorPosition;
	SetCursorPosition(Coordinates(mState.mCursorPosition.mLine, 0));

	if (mState.mCursorPosition != oldPos)
	{
		if (aSelect)
		{
			if (oldPos == mInteractiveStart)
				mInteractiveStart = mState.mCursorPosition;
			else if (oldPos == mInteractiveEnd)
				mInteractiveEnd = mState.mCursorPosition;
			else
			{
				mInteractiveStart = mState.mCursorPosition;
				mInteractiveEnd = oldPos;
			}
		}
		else
			mInteractiveStart = mInteractiveEnd = mState.mCursorPosition;
		SetSelection(mInteractiveStart, mInteractiveEnd);
	}
}

void TextEditor::MoveEnd(bool aSelect)
{
	auto oldPos = mState.mCursorPosition;
	SetCursorPosition(Coordinates(mState.mCursorPosition.mLine, GetLineMaxColumn(oldPos.mLine)));

	if (mState.mCursorPosition != oldPos)
	{
		if (aSelect)
		{
			if (oldPos == mInteractiveEnd)
				mInteractiveEnd = mState.mCursorPosition;
			else if (oldPos == mInteractiveStart)
				mInteractiveStart = mState.mCursorPosition;
			else
			{
				mInteractiveStart = oldPos;
				mInteractiveEnd = mState.mCursorPosition;
			}
		}
		else
			mInteractiveStart = mInteractiveEnd = mState.mCursorPosition;
		SetSelection(mInteractiveStart, mInteractiveEnd);
	}
}

void TextEditor::Delete()
{
	assert(!mReadOnly);

	if (mLines.empty())
		return;

	UndoRecord u;
	u.mBefore = mState;

	if (HasSelection())
	{
		u.mRemoved = GetSelectedText();
		u.mRemovedStart = mState.mSelectionStart;
		u.mRemovedEnd = mState.mSelectionEnd;

		DeleteSelection();
	}
	else
	{
		auto pos = GetActualCursorCoordinates();
		SetCursorPosition(pos);
		auto& line = mLines[pos.mLine];

		if (pos.mColumn == GetLineMaxColumn(pos.mLine))
		{
			if (pos.mLine == (int)mLines.size() - 1)
				return;

			u.mRemoved = '\n';
			u.mRemovedStart = u.mRemovedEnd = GetActualCursorCoordinates();
			Advance(u.mRemovedEnd);

			auto& nextLine = mLines[pos.mLine + 1];
			line.insert(line.end(), nextLine.begin(), nextLine.end());
			RemoveLine(pos.mLine + 1);
		}
		else
		{
			auto cindex = GetCharacterIndex(pos);
			u.mRemovedStart = u.mRemovedEnd = GetActualCursorCoordinates();
			u.mRemovedEnd.mColumn++;
			u.mRemoved = GetText(u.mRemovedStart, u.mRemovedEnd);

			auto d = UTF8CharLength(line[cindex].mChar);
			while (d-- > 0 && cindex < (int)line.size())
				line.erase(line.begin() + cindex);
		}

		mTextChanged = true;

		Colorize(pos.mLine, 1);
	}

	u.mAfter = mState;
	AddUndo(u);
}

void TextEditor::Backspace()
{
	assert(!mReadOnly);

	if (mLines.empty())
		return;

	UndoRecord u;
	u.mBefore = mState;

	if (HasSelection())
	{
		u.mRemoved = GetSelectedText();
		u.mRemovedStart = mState.mSelectionStart;
		u.mRemovedEnd = mState.mSelectionEnd;

		DeleteSelection();
	}
	else
	{
		auto pos = GetActualCursorCoordinates();
		SetCursorPosition(pos);

		if (mState.mCursorPosition.mColumn == 0)
		{
			if (mState.mCursorPosition.mLine == 0)
				return;

			u.mRemoved = '\n';
			u.mRemovedStart = u.mRemovedEnd = Coordinates(pos.mLine - 1, GetLineMaxColumn(pos.mLine - 1));
			Advance(u.mRemovedEnd);

			auto& line = mLines[mState.mCursorPosition.mLine];
			auto& prevLine = mLines[mState.mCursorPosition.mLine - 1];
			auto prevSize = GetLineMaxColumn(mState.mCursorPosition.mLine - 1);
			prevLine.insert(prevLine.end(), line.begin(), line.end());

			ErrorMarkers etmp;
			for (auto& i : mErrorMarkers)
				etmp.insert(ErrorMarkers::value_type(i.first - 1 == mState.mCursorPosition.mLine ? i.first - 1 : i.first, i.second));
			mErrorMarkers = std::move(etmp);

			RemoveLine(mState.mCursorPosition.mLine);
			--mState.mCursorPosition.mLine;
			mState.mCursorPosition.mColumn = prevSize;
		}
		else
		{
			auto& line = mLines[mState.mCursorPosition.mLine];
			auto cindex = GetCharacterIndex(pos) - 1;
			auto cend = cindex + 1;
			while (cindex > 0 && IsUTFSequence(line[cindex].mChar))
				--cindex;

			//if (cindex > 0 && UTF8CharLength(line[cindex].mChar) > 1)
			//	--cindex;

			if (mCompleteBraces && pos.mColumn < line.size()) {
				if ((line[pos.mColumn - 1].mChar == '(' && line[pos.mColumn].mChar == ')') ||
					(line[pos.mColumn - 1].mChar == '{' && line[pos.mColumn].mChar == '}') ||
					(line[pos.mColumn - 1].mChar == '[' && line[pos.mColumn].mChar == ']'))
					Delete();
			}

			u.mRemovedStart = u.mRemovedEnd = GetActualCursorCoordinates();
			--u.mRemovedStart.mColumn;
			--mState.mCursorPosition.mColumn;

			while (cindex < line.size() && cend-- > cindex)
			{
				u.mRemoved += line[cindex].mChar;
				line.erase(line.begin() + cindex);
			}
		}

		mTextChanged = true;

		EnsureCursorVisible();
		Colorize(mState.mCursorPosition.mLine, 1);
	}

	u.mAfter = mState;
	AddUndo(u);
}

void TextEditor::SelectWordUnderCursor()
{
	auto c = GetCursorPosition();
	SetSelection(FindWordStart(c), FindWordEnd(c));
}

void TextEditor::SelectAll()
{
	SetSelection(Coordinates(0, 0), Coordinates((int)mLines.size(), 0));
}

bool TextEditor::HasSelection() const
{
	return mState.mSelectionEnd > mState.mSelectionStart;
}

void TextEditor::SetShortcut(TextEditor::ShortcutID id, Shortcut s) {
	m_shortcuts[(int)id].Key1 = s.Key1;
	m_shortcuts[(int)id].Key2 = s.Key2;
	if (m_shortcuts[(int)id].Ctrl != 2)
		m_shortcuts[(int)id].Ctrl = s.Ctrl;
	if (m_shortcuts[(int)id].Shift != 2)
		m_shortcuts[(int)id].Shift = s.Shift;
	if (m_shortcuts[(int)id].Alt != 2)
		m_shortcuts[(int)id].Alt = s.Alt;
}

void TextEditor::Copy()
{
	if (HasSelection())
	{
		ImGui::SetClipboardText(GetSelectedText().c_str());
	}
	else
	{
		if (!mLines.empty())
		{
			std::string str;
			auto& line = mLines[GetActualCursorCoordinates().mLine];
			for (auto& g : line)
				str.push_back(g.mChar);
			ImGui::SetClipboardText(str.c_str());
		}
	}
}

void TextEditor::Cut()
{
	if (IsReadOnly())
	{
		Copy();
	}
	else
	{
		if (HasSelection())
		{
			UndoRecord u;
			u.mBefore = mState;
			u.mRemoved = GetSelectedText();
			u.mRemovedStart = mState.mSelectionStart;
			u.mRemovedEnd = mState.mSelectionEnd;

			Copy();
			DeleteSelection();

			u.mAfter = mState;
			AddUndo(u);
		}
	}
}

void TextEditor::Paste()
{
	if (IsReadOnly())
		return;

	auto clipText = ImGui::GetClipboardText();
	if (clipText != nullptr && strlen(clipText) > 0)
	{
		UndoRecord u;
		u.mBefore = mState;

		if (HasSelection())
		{
			u.mRemoved = GetSelectedText();
			u.mRemovedStart = mState.mSelectionStart;
			u.mRemovedEnd = mState.mSelectionEnd;
			DeleteSelection();
		}

		u.mAdded = clipText;
		u.mAddedStart = GetActualCursorCoordinates();

		InsertText(clipText);

		u.mAddedEnd = GetActualCursorCoordinates();
		u.mAfter = mState;
		AddUndo(u);
	}
}

bool TextEditor::CanUndo() const
{
	return !mReadOnly && mUndoIndex > 0;
}

bool TextEditor::CanRedo() const
{
	return !mReadOnly && mUndoIndex < (int)mUndoBuffer.size();
}

void TextEditor::Undo(int aSteps)
{
	while (CanUndo() && aSteps-- > 0)
		mUndoBuffer[--mUndoIndex].Undo(this);
}

void TextEditor::Redo(int aSteps)
{
	while (CanRedo() && aSteps-- > 0)
		mUndoBuffer[mUndoIndex++].Redo(this);
}

const TextEditor::Palette & TextEditor::GetDarkPalette()
{
	const static Palette p = { {
		0xff7f7f7f,	// Default
		0xffd69c56,	// Keyword	
		0xff00ff00,	// Number
		0xff7070e0,	// String
		0xff70a0e0, // Char literal
		0xffffffff, // Punctuation
		0xff408080,	// Preprocessor
		0xffaaaaaa, // Identifier
		0xff9bc64d, // Known identifier
		0xffc040a0, // Preproc identifier
		0xff206020, // Comment (single line)
		0xff406020, // Comment (multi line)
		0xff101010, // Background
		0xffe0e0e0, // Cursor
		0x80a06020, // Selection
		0x800020ff, // ErrorMarker
		0x40f08000, // Breakpoint
		0xff707000, // Line number
		0x40000000, // Current line fill
		0x40808080, // Current line fill (inactive)
		0x40a0a0a0, // Current line edge
		0xff33ffff, // Error message
	} };
	return p;
}

const TextEditor::Palette & TextEditor::GetLightPalette()
{
	const static Palette p = { {
		0xff7f7f7f,	// None
		0xffff0c06,	// Keyword	
		0xff008000,	// Number
		0xff2020a0,	// String
		0xff304070, // Char literal
		0xff000000, // Punctuation
		0xff406060,	// Preprocessor
		0xff404040, // Identifier
		0xff606010, // Known identifier
		0xffc040a0, // Preproc identifier
		0xff205020, // Comment (single line)
		0xff405020, // Comment (multi line)
		0xffffffff, // Background
		0xff000000, // Cursor
		0x80600000, // Selection
		0xa00010ff, // ErrorMarker
		0x80f08000, // Breakpoint
		0xff505000, // Line number
		0x40000000, // Current line fill
		0x40808080, // Current line fill (inactive)
		0x40000000, // Current line edge
		0xff3333ff, // Error message
	} };
	return p;
}

const TextEditor::Palette & TextEditor::GetRetroBluePalette()
{
	const static Palette p = { {
		0xff00ffff,	// None
		0xffffff00,	// Keyword	
		0xff00ff00,	// Number
		0xff808000,	// String
		0xff808000, // Char literal
		0xffffffff, // Punctuation
		0xff008000,	// Preprocessor
		0xff00ffff, // Identifier
		0xffffffff, // Known identifier
		0xffff00ff, // Preproc identifier
		0xff808080, // Comment (single line)
		0xff404040, // Comment (multi line)
		0xff800000, // Background
		0xff0080ff, // Cursor
		0x80ffff00, // Selection
		0xa00000ff, // ErrorMarker
		0x80ff8000, // Breakpoint
		0xff808000, // Line number
		0x40000000, // Current line fill
		0x40808080, // Current line fill (inactive)
		0x40000000, // Current line edge
		0xffffff00, // Error message
	} };
	return p;
}


std::string TextEditor::GetText() const
{
	return GetText(Coordinates(), Coordinates((int)mLines.size(), 0));
}

std::vector<std::string> TextEditor::GetTextLines() const
{
	std::vector<std::string> result;

	result.reserve(mLines.size());

	for (auto & line : mLines)
	{
		std::string text;

		text.resize(line.size());

		for (size_t i = 0; i < line.size(); ++i)
			text[i] = line[i].mChar;

		result.emplace_back(std::move(text));
	}

	return result;
}

std::string TextEditor::GetSelectedText() const
{
	return GetText(mState.mSelectionStart, mState.mSelectionEnd);
}

std::string TextEditor::GetCurrentLineText()const
{
	auto lineLength = GetLineMaxColumn(mState.mCursorPosition.mLine);
	return GetText(
		Coordinates(mState.mCursorPosition.mLine, 0),
		Coordinates(mState.mCursorPosition.mLine, lineLength));
}

void TextEditor::ProcessInputs()
{
}

void TextEditor::Colorize(int aFromLine, int aLines)
{
	int toLine = aLines == -1 ? (int)mLines.size() : std::min<int>((int)mLines.size(), aFromLine + aLines);
	mColorRangeMin = std::min<int>(mColorRangeMin, aFromLine);
	mColorRangeMax = std::max<int>(mColorRangeMax, toLine);
	mColorRangeMin = std::max<int>(0, mColorRangeMin);
	mColorRangeMax = std::max<int>(mColorRangeMin, mColorRangeMax);
	mCheckComments = true;
}

void TextEditor::ColorizeRange(int aFromLine, int aToLine)
{
	if (mLines.empty() || !mColorizerEnabled)
		return;

	std::string buffer;
	std::cmatch results;
	std::string id;

	int endLine = std::max(0, std::min((int)mLines.size(), aToLine));
	for (int i = aFromLine; i < endLine; ++i)
	{
		auto& line = mLines[i];

		if (line.empty())
			continue;

		buffer.resize(line.size());
		for (size_t j = 0; j < line.size(); ++j)
		{
			auto& col = line[j];
			buffer[j] = col.mChar;
			col.mColorIndex = PaletteIndex::Default;
		}

		const char* bufferBegin = &buffer.front();
		const char* bufferEnd = bufferBegin + buffer.size();

		auto last = bufferEnd;

		for (auto first = bufferBegin; first != last; )
		{
			const char* token_begin = nullptr;
			const char* token_end = nullptr;
			PaletteIndex token_color = PaletteIndex::Default;

			bool hasTokenizeResult = false;

			if (mLanguageDefinition.mTokenize != nullptr)
			{
				if (mLanguageDefinition.mTokenize(first, last, token_begin, token_end, token_color))
					hasTokenizeResult = true;
			}

			if (hasTokenizeResult == false)
			{
				// todo : remove
					//printf("using regex for %.*s\n", first + 10 < last ? 10 : int(last - first), first);

				for (auto& p : mRegexList)
				{
					if (std::regex_search(first, last, results, p.first, std::regex_constants::match_continuous))
					{
						hasTokenizeResult = true;

						auto& v = *results.begin();
						token_begin = v.first;
						token_end = v.second;
						token_color = p.second;
						break;
					}
				}
			}

			if (hasTokenizeResult == false)
			{
				first++;
			}
			else
			{
				const size_t token_length = token_end - token_begin;

				if (token_color == PaletteIndex::Identifier)
				{
					id.assign(token_begin, token_end);

					// todo : allmost all language definitions use lower case to specify keywords, so shouldn't this use ::tolower ?
					if (!mLanguageDefinition.mCaseSensitive)
						std::transform(id.begin(), id.end(), id.begin(), ::toupper);

					if (!line[first - bufferBegin].mPreprocessor)
					{
						if (mLanguageDefinition.mKeywords.count(id) != 0)
							token_color = PaletteIndex::Keyword;
						else if (mLanguageDefinition.mIdentifiers.count(id) != 0)
							token_color = PaletteIndex::KnownIdentifier;
						else if (mLanguageDefinition.mPreprocIdentifiers.count(id) != 0)
							token_color = PaletteIndex::PreprocIdentifier;
					}
					else
					{
						if (mLanguageDefinition.mPreprocIdentifiers.count(id) != 0)
							token_color = PaletteIndex::PreprocIdentifier;
					}
				}

				for (size_t j = 0; j < token_length; ++j)
					line[(token_begin - bufferBegin) + j].mColorIndex = token_color;

				first = token_end;
			}
		}
	}
}

void TextEditor::ColorizeInternal()
{
	if (mLines.empty() || !mColorizerEnabled)
		return;

	if (mCheckComments)
	{
		auto endLine = mLines.size();
		auto endIndex = 0;
		auto commentStartLine = endLine;
		auto commentStartIndex = endIndex;
		auto withinString = false;
		auto withinSingleLineComment = false;
		auto withinPreproc = false;
		auto firstChar = true;			// there is no other non-whitespace characters in the line before
		auto concatenate = false;		// '\' on the very end of the line
		auto currentLine = 0;
		auto currentIndex = 0;
		while (currentLine < endLine || currentIndex < endIndex)
		{
			auto& line = mLines[currentLine];

			if (currentIndex == 0 && !concatenate)
			{
				withinSingleLineComment = false;
				withinPreproc = false;
				firstChar = true;
			}

			concatenate = false;

			if (!line.empty())
			{
				auto& g = line[currentIndex];
				auto c = g.mChar;

				if (c != mLanguageDefinition.mPreprocChar && !isspace(c))
					firstChar = false;

				if (currentIndex == (int)line.size() - 1 && line[line.size() - 1].mChar == '\\')
					concatenate = true;

				bool inComment = (commentStartLine < currentLine || (commentStartLine == currentLine && commentStartIndex <= currentIndex));

				if (withinString)
				{
					line[currentIndex].mMultiLineComment = inComment;

					if (c == '\"')
					{
						if (currentIndex + 1 < (int)line.size() && line[currentIndex + 1].mChar == '\"')
						{
							currentIndex += 1;
							if (currentIndex < (int)line.size())
								line[currentIndex].mMultiLineComment = inComment;
						}
						else
							withinString = false;
					}
					else if (c == '\\')
					{
						currentIndex += 1;
						if (currentIndex < (int)line.size())
							line[currentIndex].mMultiLineComment = inComment;
					}
				}
				else
				{
					if (firstChar && c == mLanguageDefinition.mPreprocChar)
						withinPreproc = true;

					if (c == '\"')
					{
						withinString = true;
						line[currentIndex].mMultiLineComment = inComment;
					}
					else
					{
						auto pred = [](const char& a, const Glyph& b) { return a == b.mChar; };
						auto from = line.begin() + currentIndex;
						auto& startStr = mLanguageDefinition.mCommentStart;
						auto& singleStartStr = mLanguageDefinition.mSingleLineComment;

						if (singleStartStr.size() > 0 &&
							currentIndex + singleStartStr.size() <= line.size() &&
							equals(singleStartStr.begin(), singleStartStr.end(), from, from + singleStartStr.size(), pred))
						{
							withinSingleLineComment = true;
						}
						else if (!withinSingleLineComment && currentIndex + startStr.size() <= line.size() &&
							equals(startStr.begin(), startStr.end(), from, from + startStr.size(), pred))
						{
							commentStartLine = currentLine;
							commentStartIndex = currentIndex;
						}

						inComment = inComment = (commentStartLine < currentLine || (commentStartLine == currentLine && commentStartIndex <= currentIndex));

						line[currentIndex].mMultiLineComment = inComment;
						line[currentIndex].mComment = withinSingleLineComment;

						auto& endStr = mLanguageDefinition.mCommentEnd;
						if (currentIndex + 1 >= (int)endStr.size() &&
							equals(endStr.begin(), endStr.end(), from + 1 - endStr.size(), from + 1, pred))
						{
							commentStartIndex = endIndex;
							commentStartLine = endLine;
						}
					}
				}
				line[currentIndex].mPreprocessor = withinPreproc;
				currentIndex += UTF8CharLength(c);
				if (currentIndex >= (int)line.size())
				{
					currentIndex = 0;
					++currentLine;
				}
			}
			else
			{
				currentIndex = 0;
				++currentLine;
			}
		}
		mCheckComments = false;
	}

	if (mColorRangeMin < mColorRangeMax)
	{
		const int increment = (mLanguageDefinition.mTokenize == nullptr) ? 10 : 10000;
		const int to = std::min<int>(mColorRangeMin + increment, mColorRangeMax);
		ColorizeRange(mColorRangeMin, to);
		mColorRangeMin = to;

		if (mColorRangeMax == mColorRangeMin)
		{
			mColorRangeMin = std::numeric_limits<int>::max();
			mColorRangeMax = 0;
		}
		return;
	}
}

float TextEditor::TextDistanceToLineStart(const Coordinates& aFrom) const
{
	auto& line = mLines[aFrom.mLine];
	float distance = 0.0f;
	float spaceSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, " ", nullptr, nullptr).x;
	int colIndex = GetCharacterIndex(aFrom);
	for (size_t it = 0u; it < line.size() && it < colIndex; )
	{
		if (line[it].mChar == '\t')
		{
			distance = (1.0f + std::floor((1.0f + distance) / (float(mTabSize) * spaceSize))) * (float(mTabSize) * spaceSize);
			++it;
		}
		else
		{
			auto d = UTF8CharLength(line[it].mChar);
			char tempCString[7];
			int i = 0;
			for (; i < 6 && d-- > 0 && it < (int)line.size(); i++, it++)
				tempCString[i] = line[it].mChar;

			tempCString[i] = '\0';
			distance += ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, tempCString, nullptr, nullptr).x;
		}
	}

	return distance;
}

void TextEditor::EnsureCursorVisible()
{
	if (!mWithinRender)
	{
		mScrollToCursor = true;
		return;
	}

	float scrollX = ImGui::GetScrollX();
	float scrollY = ImGui::GetScrollY();

	auto height = ImGui::GetWindowHeight();
	auto width = ImGui::GetWindowWidth();

	auto top = 1 + (int)ceil(scrollY / mCharAdvance.y);
	auto bottom = (int)ceil((scrollY + height) / mCharAdvance.y);

	auto left = (int)ceil(scrollX / mCharAdvance.x);
	auto right = (int)ceil((scrollX + width) / mCharAdvance.x);

	auto pos = GetActualCursorCoordinates();
	auto len = TextDistanceToLineStart(pos);

	if (pos.mLine < top)
		ImGui::SetScrollY(std::max(0.0f, (pos.mLine - 1) * mCharAdvance.y));
	if (pos.mLine > bottom - 4)
		ImGui::SetScrollY(std::max(0.0f, (pos.mLine + 4) * mCharAdvance.y - height));
	if (len + mTextStart < left + 4)
		ImGui::SetScrollX(std::max(0.0f, len + mTextStart - 4));
	if (len + mTextStart > right - 4)
		ImGui::SetScrollX(std::max(0.0f, len + mTextStart + 4 - width));
}

int TextEditor::GetPageSize() const
{
	auto height = ImGui::GetWindowHeight() - 20.0f;
	return (int)floor(height / mCharAdvance.y);
}

TextEditor::UndoRecord::UndoRecord(
	const std::string& aAdded,
	const TextEditor::Coordinates aAddedStart,
	const TextEditor::Coordinates aAddedEnd,
	const std::string& aRemoved,
	const TextEditor::Coordinates aRemovedStart,
	const TextEditor::Coordinates aRemovedEnd,
	TextEditor::EditorState& aBefore,
	TextEditor::EditorState& aAfter)
	: mAdded(aAdded)
	, mAddedStart(aAddedStart)
	, mAddedEnd(aAddedEnd)
	, mRemoved(aRemoved)
	, mRemovedStart(aRemovedStart)
	, mRemovedEnd(aRemovedEnd)
	, mBefore(aBefore)
	, mAfter(aAfter)
{
	assert(mAddedStart <= mAddedEnd);
	assert(mRemovedStart <= mRemovedEnd);
}

void TextEditor::UndoRecord::Undo(TextEditor * aEditor)
{
	if (!mAdded.empty())
	{
		aEditor->DeleteRange(mAddedStart, mAddedEnd);
		aEditor->Colorize(mAddedStart.mLine - 1, mAddedEnd.mLine - mAddedStart.mLine + 2);
	}

	if (!mRemoved.empty())
	{
		auto start = mRemovedStart;
		aEditor->InsertTextAt(start, mRemoved.c_str());
		aEditor->Colorize(mRemovedStart.mLine - 1, mRemovedEnd.mLine - mRemovedStart.mLine + 2);
	}

	aEditor->mState = mBefore;
	aEditor->EnsureCursorVisible();

}

void TextEditor::UndoRecord::Redo(TextEditor * aEditor)
{
	if (!mRemoved.empty())
	{
		aEditor->DeleteRange(mRemovedStart, mRemovedEnd);
		aEditor->Colorize(mRemovedStart.mLine - 1, mRemovedEnd.mLine - mRemovedStart.mLine + 1);
	}

	if (!mAdded.empty())
	{
		auto start = mAddedStart;
		aEditor->InsertTextAt(start, mAdded.c_str());
		aEditor->Colorize(mAddedStart.mLine - 1, mAddedEnd.mLine - mAddedStart.mLine + 1);
	}

	aEditor->mState = mAfter;
	aEditor->EnsureCursorVisible();
}

static bool TokenizeCStyleString(const char* in_begin, const char* in_end, const char*& out_begin, const char*& out_end)
{
	const char* p = in_begin;

	if (*p == '"')
	{
		p++;

		while (p < in_end)
		{
			// handle end of string
			if (*p == '"')
			{
				out_begin = in_begin;
				out_end = p + 1;
				return true;
			}

			// handle escape character for "
			if (*p == '\\' && p + 1 < in_end && p[1] == '"')
				p++;

			p++;
		}
	}

	return false;
}

static bool TokenizeCStyleCharacterLiteral(const char* in_begin, const char* in_end, const char*& out_begin, const char*& out_end)
{
	const char* p = in_begin;

	if (*p == '\'')
	{
		p++;

		// handle escape characters
		if (p < in_end && *p == '\\')
			p++;

		if (p < in_end)
			p++;

		// handle end of character literal
		if (p < in_end && *p == '\'')
		{
			out_begin = in_begin;
			out_end = p + 1;
			return true;
		}
	}

	return false;
}

static bool TokenizeCStyleIdentifier(const char* in_begin, const char* in_end, const char*& out_begin, const char*& out_end)
{
	const char* p = in_begin;

	if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || *p == '_')
	{
		p++;

		while ((p < in_end) && ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_'))
			p++;

		out_begin = in_begin;
		out_end = p;
		return true;
	}

	return false;
}

static bool TokenizeCStyleNumber(const char* in_begin, const char* in_end, const char*& out_begin, const char*& out_end)
{
	const char* p = in_begin;

	const bool startsWithNumber = *p >= '0' && *p <= '9';

	if (*p != '+' && *p != '-' && !startsWithNumber)
		return false;

	p++;

	bool hasNumber = startsWithNumber;

	while (p < in_end && (*p >= '0' && *p <= '9'))
	{
		hasNumber = true;

		p++;
	}

	if (hasNumber == false)
		return false;

	bool isFloat = false;
	bool isHex = false;
	bool isBinary = false;

	if (p < in_end)
	{
		if (*p == '.')
		{
			isFloat = true;

			p++;

			while (p < in_end && (*p >= '0' && *p <= '9'))
				p++;
		}
		else if (*p == 'x' || *p == 'X')
		{
			// hex formatted integer of the type 0xef80

			isHex = true;

			p++;

			while (p < in_end && ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F')))
				p++;
		}
		else if (*p == 'b' || *p == 'B')
		{
			// binary formatted integer of the type 0b01011101

			isBinary = true;

			p++;

			while (p < in_end && (*p >= '0' && *p <= '1'))
				p++;
		}
	}

	if (isHex == false && isBinary == false)
	{
		// floating point exponent
		if (p < in_end && (*p == 'e' || *p == 'E'))
		{
			isFloat = true;

			p++;

			if (p < in_end && (*p == '+' || *p == '-'))
				p++;

			bool hasDigits = false;

			while (p < in_end && (*p >= '0' && *p <= '9'))
			{
				hasDigits = true;

				p++;
			}

			if (hasDigits == false)
				return false;
		}

		// single precision floating point type
		if (p < in_end && *p == 'f')
			p++;
	}

	if (isFloat == false)
	{
		// integer size type
		while (p < in_end && (*p == 'u' || *p == 'U' || *p == 'l' || *p == 'L'))
			p++;
	}

	out_begin = in_begin;
	out_end = p;
	return true;
}

static bool TokenizeCStylePunctuation(const char * in_begin, const char * in_end, const char *& out_begin, const char *& out_end)
{
	(void)in_end;

	switch (*in_begin)
	{
	case '[':
	case ']':
	case '{':
	case '}':
	case '!':
	case '%':
	case '^':
	case '&':
	case '*':
	case '(':
	case ')':
	case '-':
	case '+':
	case '=':
	case '~':
	case '|':
	case '<':
	case '>':
	case '?':
	case ':':
	case '/':
	case ';':
	case ',':
	case '.':
		out_begin = in_begin;
		out_end = in_begin + 1;
		return true;
	}

	return false;
}

const TextEditor::LanguageDefinition& TextEditor::LanguageDefinition::CPlusPlus()
{
	static bool inited = false;
	static LanguageDefinition langDef;
	if (!inited)
	{
		static const char* const cppKeywords[] = {
			"alignas", "alignof", "and", "and_eq", "asm", "atomic_cancel", "atomic_commit", "atomic_noexcept", "auto", "bitand", "bitor", "bool", "break", "case", "catch", "char", "char16_t", "char32_t", "class",
			"compl", "concept", "const", "constexpr", "const_cast", "continue", "decltype", "default", "delete", "do", "double", "dynamic_cast", "else", "enum", "explicit", "export", "extern", "false", "float",
			"for", "friend", "goto", "if", "import", "inline", "int", "long", "module", "mutable", "namespace", "new", "noexcept", "not", "not_eq", "nullptr", "operator", "or", "or_eq", "private", "protected", "public",
			"register", "reinterpret_cast", "requires", "return", "short", "signed", "sizeof", "static", "static_assert", "static_cast", "struct", "switch", "synchronized", "template", "this", "thread_local",
			"throw", "true", "try", "typedef", "typeid", "typename", "union", "unsigned", "using", "virtual", "void", "volatile", "wchar_t", "while", "xor", "xor_eq"
		};
		for (auto& k : cppKeywords)
			langDef.mKeywords.insert(k);

		static const char* const identifiers[] = {
			"abort", "abs", "acos", "asin", "atan", "atexit", "atof", "atoi", "atol", "ceil", "clock", "cosh", "ctime", "div", "exit", "fabs", "floor", "fmod", "getchar", "getenv", "isalnum", "isalpha", "isdigit", "isgraph",
			"ispunct", "isspace", "isupper", "kbhit", "log10", "log2", "log", "memcmp", "modf", "pow", "printf", "sprintf", "snprintf", "putchar", "putenv", "puts", "rand", "remove", "rename", "sinh", "sqrt", "srand", "strcat", "strcmp", "strerror", "time", "tolower", "toupper",
			"std", "string", "vector", "map", "unordered_map", "set", "unordered_set", "min", "max"
		};
		for (auto& k : identifiers)
		{
			Identifier id;
			id.mDeclaration = "Built-in function";
			langDef.mIdentifiers.insert(std::make_pair(std::string(k), id));
		}

		langDef.mTokenize = [](const char * in_begin, const char * in_end, const char *& out_begin, const char *& out_end, PaletteIndex & paletteIndex) -> bool
		{
			paletteIndex = PaletteIndex::Max;

			while (in_begin < in_end && isascii(*in_begin) && isblank(*in_begin))
				in_begin++;

			if (in_begin == in_end)
			{
				out_begin = in_end;
				out_end = in_end;
				paletteIndex = PaletteIndex::Default;
			}
			else if (TokenizeCStyleString(in_begin, in_end, out_begin, out_end))
				paletteIndex = PaletteIndex::String;
			else if (TokenizeCStyleCharacterLiteral(in_begin, in_end, out_begin, out_end))
				paletteIndex = PaletteIndex::CharLiteral;
			else if (TokenizeCStyleIdentifier(in_begin, in_end, out_begin, out_end))
				paletteIndex = PaletteIndex::Identifier;
			else if (TokenizeCStyleNumber(in_begin, in_end, out_begin, out_end))
				paletteIndex = PaletteIndex::Number;
			else if (TokenizeCStylePunctuation(in_begin, in_end, out_begin, out_end))
				paletteIndex = PaletteIndex::Punctuation;

			return paletteIndex != PaletteIndex::Max;
		};

		langDef.mCommentStart = "/*";
		langDef.mCommentEnd = "*/";
		langDef.mSingleLineComment = "//";

		langDef.mCaseSensitive = true;
		langDef.mAutoIndentation = true;

		langDef.mName = "C++";

		inited = true;
	}
	return langDef;
}

const TextEditor::LanguageDefinition& TextEditor::LanguageDefinition::HLSL()
{
	static bool inited = false;
	static LanguageDefinition langDef;
	if (!inited)
	{
		static const char* const keywords[] = {
			"AppendStructuredBuffer", "asm", "asm_fragment", "BlendState", "bool", "break", "Buffer", "ByteAddressBuffer", "case", "cbuffer", "centroid", "class", "column_major", "compile", "compile_fragment",
			"CompileShader", "const", "continue", "ComputeShader", "ConsumeStructuredBuffer", "default", "DepthStencilState", "DepthStencilView", "discard", "do", "double", "DomainShader", "dword", "else",
			"export", "extern", "false", "float", "for", "fxgroup", "GeometryShader", "groupshared", "half", "Hullshader", "if", "in", "inline", "inout", "InputPatch", "int", "interface", "line", "lineadj",
			"linear", "LineStream", "matrix", "min16float", "min10float", "min16int", "min12int", "min16uint", "namespace", "nointerpolation", "noperspective", "NULL", "out", "OutputPatch", "packoffset",
			"pass", "pixelfragment", "PixelShader", "point", "PointStream", "precise", "RasterizerState", "RenderTargetView", "return", "register", "row_major", "RWBuffer", "RWByteAddressBuffer", "RWStructuredBuffer",
			"RWTexture1D", "RWTexture1DArray", "RWTexture2D", "RWTexture2DArray", "RWTexture3D", "sample", "sampler", "SamplerState", "SamplerComparisonState", "shared", "snorm", "stateblock", "stateblock_state",
			"static", "string", "struct", "switch", "StructuredBuffer", "tbuffer", "technique", "technique10", "technique11", "texture", "Texture1D", "Texture1DArray", "Texture2D", "Texture2DArray", "Texture2DMS",
			"Texture2DMSArray", "Texture3D", "TextureCube", "TextureCubeArray", "true", "typedef", "triangle", "triangleadj", "TriangleStream", "uint", "uniform", "unorm", "unsigned", "vector", "vertexfragment",
			"VertexShader", "void", "volatile", "while",
			"bool1","bool2","bool3","bool4","double1","double2","double3","double4", "float1", "float2", "float3", "float4", "int1", "int2", "int3", "int4", "in", "out", "inout",
			"uint1", "uint2", "uint3", "uint4", "dword1", "dword2", "dword3", "dword4", "half1", "half2", "half3", "half4",
			"float1x1","float2x1","float3x1","float4x1","float1x2","float2x2","float3x2","float4x2",
			"float1x3","float2x3","float3x3","float4x3","float1x4","float2x4","float3x4","float4x4",
			"half1x1","half2x1","half3x1","half4x1","half1x2","half2x2","half3x2","half4x2",
			"half1x3","half2x3","half3x3","half4x3","half1x4","half2x4","half3x4","half4x4"
		};
		for (auto& k : keywords)
			langDef.mKeywords.insert(k);

		m_HLSLDocumentation(langDef.mIdentifiers);

		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[ \\t]*#[ \\t]*[a-zA-Z_]+", PaletteIndex::Preprocessor));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::String));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("\\'\\\\?[^\\']\\'", PaletteIndex::CharLiteral));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/\\;\\,\\.]", PaletteIndex::Punctuation));

		langDef.mCommentStart = "/*";
		langDef.mCommentEnd = "*/";
		langDef.mSingleLineComment = "//";

		langDef.mCaseSensitive = true;
		langDef.mAutoIndentation = true;

		langDef.mName = "HLSL";

		inited = true;
	}
	return langDef;
}
void TextEditor::LanguageDefinition::m_HLSLDocumentation(Identifiers& idents)
{
	std::function<Identifier(const std::string&)> desc = [](const std::string & str) {
		Identifier id;
		id.mDeclaration = str;
		return id;
	};

	/* SOURCE: https://docs.microsoft.com/en-us/windows/desktop/direct3dhlsl/dx-graphics-hlsl-intrinsic-functions */

	idents.insert(std::make_pair("abort", desc("Terminates the current draw or dispatch call being executed.")));
	idents.insert(std::make_pair("abs", desc("Absolute value (per component).")));
	idents.insert(std::make_pair("acos", desc("Returns the arccosine of each component of x.")));
	idents.insert(std::make_pair("all", desc("Test if all components of x are nonzero.")));
	idents.insert(std::make_pair("AllMemoryBarrier", desc("Blocks execution of all threads in a group until all memory accesses have been completed.")));
	idents.insert(std::make_pair("AllMemoryBarrierWithGroupSync", desc("Blocks execution of all threads in a group until all memory accesses have been completed and all threads in the group have reached this call.")));
	idents.insert(std::make_pair("any", desc("Test if any component of x is nonzero.")));
	idents.insert(std::make_pair("asdouble", desc("Reinterprets a cast value into a double.")));
	idents.insert(std::make_pair("asfloat", desc("Convert the input type to a float.")));
	idents.insert(std::make_pair("asin", desc("Returns the arcsine of each component of x.")));
	idents.insert(std::make_pair("asint", desc("Convert the input type to an integer.")));
	idents.insert(std::make_pair("asuint", desc("Convert the input type to an unsigned integer.")));
	idents.insert(std::make_pair("atan", desc("Returns the arctangent of x.")));
	idents.insert(std::make_pair("atan2", desc("Returns the arctangent of of two values (x,y).")));
	idents.insert(std::make_pair("ceil", desc("Returns the smallest integer which is greater than or equal to x.")));
	idents.insert(std::make_pair("CheckAccessFullyMapped", desc("Determines whether all values from a Sample or Load operation accessed mapped tiles in a tiled resource.")));
	idents.insert(std::make_pair("clamp", desc("Clamps x to the range [min, max].")));
	idents.insert(std::make_pair("clip", desc("Discards the current pixel, if any component of x is less than zero.")));
	idents.insert(std::make_pair("cos", desc("Returns the cosine of x.")));
	idents.insert(std::make_pair("cosh", desc("Returns the hyperbolic cosine of x.")));
	idents.insert(std::make_pair("countbits", desc("Counts the number of bits (per component) in the input integer.")));
	idents.insert(std::make_pair("cross", desc("Returns the cross product of two 3D vectors.")));
	idents.insert(std::make_pair("D3DCOLORtoUBYTE4", desc("Swizzles and scales components of the 4D vector x to compensate for the lack of UBYTE4 support in some hardware.")));
	idents.insert(std::make_pair("ddx", desc("Returns the partial derivative of x with respect to the screen-space x-coordinate.")));
	idents.insert(std::make_pair("ddx_coarse", desc("Computes a low precision partial derivative with respect to the screen-space x-coordinate.")));
	idents.insert(std::make_pair("ddx_fine", desc("Computes a high precision partial derivative with respect to the screen-space x-coordinate.")));
	idents.insert(std::make_pair("ddy", desc("Returns the partial derivative of x with respect to the screen-space y-coordinate.")));
	idents.insert(std::make_pair("ddy_coarse", desc("Returns the partial derivative of x with respect to the screen-space y-coordinate.")));
	idents.insert(std::make_pair("ddy_fine", desc("Computes a high precision partial derivative with respect to the screen-space y-coordinate.")));
	idents.insert(std::make_pair("degrees", desc("Converts x from radians to degrees.")));
	idents.insert(std::make_pair("determinant", desc("Returns the determinant of the square matrix m.")));
	idents.insert(std::make_pair("DeviceMemoryBarrier", desc("Blocks execution of all threads in a group until all device memory accesses have been completed.")));
	idents.insert(std::make_pair("DeviceMemoryBarrierWithGroupSync", desc("Blocks execution of all threads in a group until all device memory accesses have been completed and all threads in the group have reached this call.")));
	idents.insert(std::make_pair("distance", desc("Returns the distance between two points.")));
	idents.insert(std::make_pair("dot", desc("Returns the dot product of two vectors.")));
	idents.insert(std::make_pair("dst", desc("Calculates a distance vector.")));
	idents.insert(std::make_pair("errorf", desc("Submits an error message to the information queue.")));
	idents.insert(std::make_pair("EvaluateAttributeAtCentroid", desc("Evaluates at the pixel centroid.")));
	idents.insert(std::make_pair("EvaluateAttributeAtSample", desc("Evaluates at the indexed sample location.")));
	idents.insert(std::make_pair("EvaluateAttributeSnapped", desc("Evaluates at the pixel centroid with an offset.")));
	idents.insert(std::make_pair("exp", desc("Returns the base-e exponent.")));
	idents.insert(std::make_pair("exp2", desc("Base 2 exponent(per component).")));
	idents.insert(std::make_pair("f16tof32", desc("Converts the float16 stored in the low-half of the uint to a float.")));
	idents.insert(std::make_pair("f32tof16", desc("Converts an input into a float16 type.")));
	idents.insert(std::make_pair("faceforward", desc("Returns -n * sign(dot(i, ng)).")));
	idents.insert(std::make_pair("firstbithigh", desc("Gets the location of the first set bit starting from the highest order bit and working downward, per component.")));
	idents.insert(std::make_pair("firstbitlow", desc("Returns the location of the first set bit starting from the lowest order bit and working upward, per component.")));
	idents.insert(std::make_pair("floor", desc("Returns the greatest integer which is less than or equal to x.")));
	idents.insert(std::make_pair("fma", desc("Returns the double-precision fused multiply-addition of a * b + c.")));
	idents.insert(std::make_pair("fmod", desc("Returns the floating point remainder of x/y.")));
	idents.insert(std::make_pair("frac", desc("Returns the fractional part of x.")));
	idents.insert(std::make_pair("frexp", desc("Returns the mantissa and exponent of x.")));
	idents.insert(std::make_pair("fwidth", desc("Returns abs(ddx(x)) + abs(ddy(x))")));
	idents.insert(std::make_pair("GetRenderTargetSampleCount", desc("Returns the number of render-target samples.")));
	idents.insert(std::make_pair("GetRenderTargetSamplePosition", desc("Returns a sample position (x,y) for a given sample index.")));
	idents.insert(std::make_pair("GroupMemoryBarrier", desc("Blocks execution of all threads in a group until all group shared accesses have been completed.")));
	idents.insert(std::make_pair("GroupMemoryBarrierWithGroupSync", desc("Blocks execution of all threads in a group until all group shared accesses have been completed and all threads in the group have reached this call.")));
	idents.insert(std::make_pair("InterlockedAdd", desc("Performs a guaranteed atomic add of value to the dest resource variable.")));
	idents.insert(std::make_pair("InterlockedAnd", desc("Performs a guaranteed atomic and.")));
	idents.insert(std::make_pair("InterlockedCompareExchange", desc("Atomically compares the input to the comparison value and exchanges the result.")));
	idents.insert(std::make_pair("InterlockedCompareStore", desc("Atomically compares the input to the comparison value.")));
	idents.insert(std::make_pair("InterlockedExchange", desc("Assigns value to dest and returns the original value.")));
	idents.insert(std::make_pair("InterlockedMax", desc("Performs a guaranteed atomic max.")));
	idents.insert(std::make_pair("InterlockedMin", desc("Performs a guaranteed atomic min.")));
	idents.insert(std::make_pair("InterlockedOr", desc("Performs a guaranteed atomic or.")));
	idents.insert(std::make_pair("InterlockedXor", desc("Performs a guaranteed atomic xor.")));
	idents.insert(std::make_pair("isfinite", desc("Returns true if x is finite, false otherwise.")));
	idents.insert(std::make_pair("isinf", desc("Returns true if x is +INF or -INF, false otherwise.")));
	idents.insert(std::make_pair("isnan", desc("Returns true if x is NAN or QNAN, false otherwise.")));
	idents.insert(std::make_pair("ldexp", desc("Returns x * 2exp")));
	idents.insert(std::make_pair("length", desc("Returns the length of the vector v.")));
	idents.insert(std::make_pair("lerp", desc("Returns x + s(y - x).")));
	idents.insert(std::make_pair("lit", desc("Returns a lighting vector (ambient, diffuse, specular, 1)")));
	idents.insert(std::make_pair("log", desc("Returns the base-e logarithm of x.")));
	idents.insert(std::make_pair("log10", desc("Returns the base-10 logarithm of x.")));
	idents.insert(std::make_pair("log2", desc("Returns the base - 2 logarithm of x.")));
	idents.insert(std::make_pair("mad", desc("Performs an arithmetic multiply/add operation on three values.")));
	idents.insert(std::make_pair("max", desc("Selects the greater of x and y.")));
	idents.insert(std::make_pair("min", desc("Selects the lesser of x and y.")));
	idents.insert(std::make_pair("modf", desc("Splits the value x into fractional and integer parts.")));
	idents.insert(std::make_pair("msad4", desc("Compares a 4-byte reference value and an 8-byte source value and accumulates a vector of 4 sums.")));
	idents.insert(std::make_pair("mul", desc("Performs matrix multiplication using x and y.")));
	idents.insert(std::make_pair("noise", desc("Generates a random value using the Perlin-noise algorithm.")));
	idents.insert(std::make_pair("normalize", desc("Returns a normalized vector.")));
	idents.insert(std::make_pair("pow", desc("Returns x^n.")));
	idents.insert(std::make_pair("printf", desc("Submits a custom shader message to the information queue.")));
	idents.insert(std::make_pair("Process2DQuadTessFactorsAvg", desc("Generates the corrected tessellation factors for a quad patch.")));
	idents.insert(std::make_pair("Process2DQuadTessFactorsMax", desc("Generates the corrected tessellation factors for a quad patch.")));
	idents.insert(std::make_pair("Process2DQuadTessFactorsMin", desc("Generates the corrected tessellation factors for a quad patch.")));
	idents.insert(std::make_pair("ProcessIsolineTessFactors", desc("Generates the rounded tessellation factors for an isoline.")));
	idents.insert(std::make_pair("ProcessQuadTessFactorsAvg", desc("Generates the corrected tessellation factors for a quad patch.")));
	idents.insert(std::make_pair("ProcessQuadTessFactorsMax", desc("Generates the corrected tessellation factors for a quad patch.")));
	idents.insert(std::make_pair("ProcessQuadTessFactorsMin", desc("Generates the corrected tessellation factors for a quad patch.")));
	idents.insert(std::make_pair("ProcessTriTessFactorsAvg", desc("Generates the corrected tessellation factors for a tri patch.")));
	idents.insert(std::make_pair("ProcessTriTessFactorsMax", desc("Generates the corrected tessellation factors for a tri patch.")));
	idents.insert(std::make_pair("ProcessTriTessFactorsMin", desc("Generates the corrected tessellation factors for a tri patch.")));
	idents.insert(std::make_pair("radians", desc("Converts x from degrees to radians.")));
	idents.insert(std::make_pair("rcp", desc("Calculates a fast, approximate, per-component reciprocal.")));
	idents.insert(std::make_pair("reflect", desc("Returns a reflection vector.")));
	idents.insert(std::make_pair("refract", desc("Returns the refraction vector.")));
	idents.insert(std::make_pair("reversebits", desc("Reverses the order of the bits, per component.")));
	idents.insert(std::make_pair("round", desc("Rounds x to the nearest integer")));
	idents.insert(std::make_pair("rsqrt", desc("Returns 1 / sqrt(x)")));
	idents.insert(std::make_pair("saturate", desc("Clamps x to the range [0, 1]")));
	idents.insert(std::make_pair("sign", desc("Computes the sign of x.")));
	idents.insert(std::make_pair("sin", desc("Returns the sine of x")));
	idents.insert(std::make_pair("sincos", desc("Returns the sineand cosine of x.")));
	idents.insert(std::make_pair("sinh", desc("Returns the hyperbolic sine of x")));
	idents.insert(std::make_pair("smoothstep", desc("Returns a smooth Hermite interpolation between 0 and 1.")));
	idents.insert(std::make_pair("sqrt", desc("Square root (per component)")));
	idents.insert(std::make_pair("step", desc("Returns (x >= a) ? 1 : 0")));
	idents.insert(std::make_pair("tan", desc("Returns the tangent of x")));
	idents.insert(std::make_pair("tanh", desc("Returns the hyperbolic tangent of x")));
	idents.insert(std::make_pair("tex1D", desc("1D texture lookup.")));
	idents.insert(std::make_pair("tex1Dbias", desc("1D texture lookup with bias.")));
	idents.insert(std::make_pair("tex1Dgrad", desc("1D texture lookup with a gradient.")));
	idents.insert(std::make_pair("tex1Dlod", desc("1D texture lookup with LOD.")));
	idents.insert(std::make_pair("tex1Dproj", desc("1D texture lookup with projective divide.")));
	idents.insert(std::make_pair("tex2D", desc("2D texture lookup.")));
	idents.insert(std::make_pair("tex2Dbias", desc("2D texture lookup with bias.")));
	idents.insert(std::make_pair("tex2Dgrad", desc("2D texture lookup with a gradient.")));
	idents.insert(std::make_pair("tex2Dlod", desc("2D texture lookup with LOD.")));
	idents.insert(std::make_pair("tex2Dproj", desc("2D texture lookup with projective divide.")));
	idents.insert(std::make_pair("tex3D", desc("3D texture lookup.")));
	idents.insert(std::make_pair("tex3Dbias", desc("3D texture lookup with bias.")));
	idents.insert(std::make_pair("tex3Dgrad", desc("3D texture lookup with a gradient.")));
	idents.insert(std::make_pair("tex3Dlod", desc("3D texture lookup with LOD.")));
	idents.insert(std::make_pair("tex3Dproj", desc("3D texture lookup with projective divide.")));
	idents.insert(std::make_pair("texCUBE", desc("Cube texture lookup.")));
	idents.insert(std::make_pair("texCUBEbias", desc("Cube texture lookup with bias.")));
	idents.insert(std::make_pair("texCUBEgrad", desc("Cube texture lookup with a gradient.")));
	idents.insert(std::make_pair("texCUBElod", desc("Cube texture lookup with LOD.")));
	idents.insert(std::make_pair("texCUBEproj", desc("Cube texture lookup with projective divide.")));
	idents.insert(std::make_pair("transpose", desc("Returns the transpose of the matrix m.")));
	idents.insert(std::make_pair("trunc", desc("Truncates floating-point value(s) to integer value(s)")));
}

const TextEditor::LanguageDefinition& TextEditor::LanguageDefinition::GLSL()
{
	static bool inited = false;
	static LanguageDefinition langDef;
	if (!inited)
	{
		static const char* const keywords[] = {
			"auto", "break", "case", "char", "const", "continue", "default", "do", "double", "else", "enum", "extern", "float", "for", "goto", "if", "inline", "int", "long", "register", "restrict", "return", "short",
			"signed", "sizeof", "static", "struct", "switch", "typedef", "union", "unsigned", "void", "volatile", "while", "_Alignas", "_Alignof", "_Atomic", "_Bool", "_Complex", "_Generic", "_Imaginary",
			"_Noreturn", "_Static_assert", "_Thread_local", "attribute", "uniform", "varying", "layout", "centroid", "flat", "smooth", "noperspective", "patch", "sample", "subroutine", "in", "out", "inout",
			"bool", "true", "false", "invariant", "mat2", "mat3", "mat4", "dmat2", "dmat3", "dmat4", "mat2x2", "mat2x3", "mat2x4", "dmat2x2", "dmat2x3", "dmat2x4", "mat3x2", "mat3x3", "mat3x4", "dmat3x2", "dmat3x3", "dmat3x4",
			"mat4x2", "mat4x3", "mat4x4", "dmat4x2", "dmat4x3", "dmat4x4", "vec2", "vec3", "vec4", "ivec2", "ivec3", "ivec4", "bvec2", "bvec3", "bvec4", "dvec2", "dvec3", "dvec4", "uint", "uvec2", "uvec3", "uvec4",
			"lowp", "mediump", "highp", "precision", "sampler1D", "sampler2D", "sampler3D", "samplerCube", "sampler1DShadow", "sampler2DShadow", "samplerCubeShadow", "sampler1DArray", "sampler2DArray", "sampler1DArrayShadow",
			"sampler2DArrayShadow", "isampler1D", "isampler2D", "isampler3D", "isamplerCube", "isampler1DArray", "isampler2DArray", "usampler1D", "usampler2D", "usampler3D", "usamplerCube", "usampler1DArray", "usampler2DArray",
			"sampler2DRect", "sampler2DRectShadow", "isampler2DRect", "usampler2DRect", "samplerBuffer", "isamplerBuffer", "usamplerBuffer", "sampler2DMS", "isampler2DMS", "usampler2DMS", "sampler2DMSArray", "isampler2DMSArray",
			"usampler2DMSArray", "samplerCubeArray", "samplerCubeArrayShadow", "isamplerCubeArray", "usamplerCubeArray"
		};
		for (auto& k : keywords)
			langDef.mKeywords.insert(k);

		m_GLSLDocumentation(langDef.mIdentifiers);

		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[ \\t]*#[ \\t]*[a-zA-Z_]+", PaletteIndex::Preprocessor));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::String));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("\\'\\\\?[^\\']\\'", PaletteIndex::CharLiteral));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/\\;\\,\\.]", PaletteIndex::Punctuation));

		langDef.mCommentStart = "/*";
		langDef.mCommentEnd = "*/";
		langDef.mSingleLineComment = "//";

		langDef.mCaseSensitive = true;
		langDef.mAutoIndentation = true;

		langDef.mName = "GLSL";

		inited = true;
	}
	return langDef;
}
void TextEditor::LanguageDefinition::m_GLSLDocumentation(Identifiers& idents)
{
	std::function<Identifier(const std::string&)> desc = [](const std::string & str) {
		Identifier id;
		id.mDeclaration = str;
		return id;
	};

	/* SOURCE: https://docs.microsoft.com/en-us/windows/desktop/direct3dhlsl/dx-graphics-hlsl-intrinsic-functions */

	idents.insert(std::make_pair("radians", desc("Converts x from degrees to radians.")));
	idents.insert(std::make_pair("degrees", desc("Converts x from radians to degrees.")));
	idents.insert(std::make_pair("sin", desc("Returns the sine of x")));
	idents.insert(std::make_pair("cos", desc("Returns the cosine of x.")));
	idents.insert(std::make_pair("tan", desc("Returns the tangent of x")));
	idents.insert(std::make_pair("asin", desc("Returns the arcsine of each component of x.")));
	idents.insert(std::make_pair("acos", desc("Returns the arccosine of each component of x.")));
	idents.insert(std::make_pair("atan", desc("Returns the arctangent of x.")));
	idents.insert(std::make_pair("sinh", desc("Returns the hyperbolic sine of x")));
	idents.insert(std::make_pair("cosh", desc("Returns the hyperbolic cosine of x.")));
	idents.insert(std::make_pair("tanh", desc("Returns the hyperbolic tangent of x")));
	idents.insert(std::make_pair("asinh", desc("Returns the arc hyperbolic sine of x")));
	idents.insert(std::make_pair("acosh", desc("Returns the arc hyperbolic cosine of x.")));
	idents.insert(std::make_pair("atanh", desc("Returns the arc hyperbolic tangent of x")));
	idents.insert(std::make_pair("pow", desc("Returns x^n.")));
	idents.insert(std::make_pair("exp", desc("Returns the base-e exponent.")));
	idents.insert(std::make_pair("exp2", desc("Base 2 exponent(per component).")));
	idents.insert(std::make_pair("log", desc("Returns the base-e logarithm of x.")));
	idents.insert(std::make_pair("log2", desc("Returns the base - 2 logarithm of x.")));
	idents.insert(std::make_pair("sqrt", desc("Square root (per component).")));
	idents.insert(std::make_pair("inversesqrt", desc("Returns rcp(sqrt(x)).")));
	idents.insert(std::make_pair("abs", desc("Absolute value (per component).")));
	idents.insert(std::make_pair("sign", desc("Computes the sign of x.")));
	idents.insert(std::make_pair("floor", desc("Returns the greatest integer which is less than or equal to x.")));
	idents.insert(std::make_pair("trunc", desc("Truncates floating-point value(s) to integer value(s)")));
	idents.insert(std::make_pair("round", desc("Rounds x to the nearest integer")));
	idents.insert(std::make_pair("roundEven", desc("Returns a value equal to the nearest integer to x. A fractional part of 0.5 will round toward the nearest even integer.")));
	idents.insert(std::make_pair("ceil", desc("Returns the smallest integer which is greater than or equal to x.")));
	idents.insert(std::make_pair("fract", desc("Returns the fractional part of x.")));
	idents.insert(std::make_pair("mod", desc("Modulus. Returns x – y ∗ floor (x/y).")));
	idents.insert(std::make_pair("modf", desc("Splits the value x into fractional and integer parts.")));
	idents.insert(std::make_pair("max", desc("Selects the greater of x and y.")));
	idents.insert(std::make_pair("min", desc("Selects the lesser of x and y.")));
	idents.insert(std::make_pair("clamp", desc("Clamps x to the range [min, max].")));
	idents.insert(std::make_pair("mix", desc("Returns x*(1-a)+y*a.")));
	idents.insert(std::make_pair("isinf", desc("Returns true if x is +INF or -INF, false otherwise.")));
	idents.insert(std::make_pair("isnan", desc("Returns true if x is NAN or QNAN, false otherwise.")));
	idents.insert(std::make_pair("smoothstep", desc("Returns a smooth Hermite interpolation between 0 and 1.")));
	idents.insert(std::make_pair("step", desc("Returns (x >= a) ? 1 : 0")));
	idents.insert(std::make_pair("floatBitsToInt", desc("Returns a signed or unsigned integer value representing the encoding of a floating-point value. The floatingpoint value's bit-level representation is preserved.")));
	idents.insert(std::make_pair("floatBitsToUint", desc("Returns a signed or unsigned integer value representing the encoding of a floating-point value. The floatingpoint value's bit-level representation is preserved.")));
	idents.insert(std::make_pair("intBitsToFloat", desc("Returns a floating-point value corresponding to a signed or unsigned integer encoding of a floating-point value.")));
	idents.insert(std::make_pair("uintBitsToFloat", desc("Returns a floating-point value corresponding to a signed or unsigned integer encoding of a floating-point value.")));
	idents.insert(std::make_pair("fmod", desc("Returns the floating point remainder of x/y.")));
	idents.insert(std::make_pair("fma", desc("Returns the double-precision fused multiply-addition of a * b + c.")));
	idents.insert(std::make_pair("ldexp", desc("Returns x * 2exp")));
	idents.insert(std::make_pair("packUnorm2x16", desc("First, converts each component of the normalized floating - point value v into 8 or 16bit integer values. Then, the results are packed into the returned 32bit unsigned integer.")));
	idents.insert(std::make_pair("packUnorm4x8", desc("First, converts each component of the normalized floating - point value v into 8 or 16bit integer values. Then, the results are packed into the returned 32bit unsigned integer.")));
	idents.insert(std::make_pair("packSnorm4x8", desc("First, converts each component of the normalized floating - point value v into 8 or 16bit integer values. Then, the results are packed into the returned 32bit unsigned integer.")));
	idents.insert(std::make_pair("unpackUnorm2x16", desc("First, unpacks a single 32bit unsigned integer p into a pair of 16bit unsigned integers, four 8bit unsigned integers, or four 8bit signed integers.Then, each component is converted to a normalized floating point value to generate the returned two or four component vector.")));
	idents.insert(std::make_pair("unpackUnorm4x8", desc("First, unpacks a single 32bit unsigned integer p into a pair of 16bit unsigned integers, four 8bit unsigned integers, or four 8bit signed integers.Then, each component is converted to a normalized floating point value to generate the returned two or four component vector.")));
	idents.insert(std::make_pair("unpackSnorm4x8", desc("First, unpacks a single 32bit unsigned integer p into a pair of 16bit unsigned integers, four 8bit unsigned integers, or four 8bit signed integers.Then, each component is converted to a normalized floating point value to generate the returned two or four component vector.")));
	idents.insert(std::make_pair("packDouble2x32", desc("Returns a double-precision value obtained by packing the components of v into a 64-bit value.")));
	idents.insert(std::make_pair("unpackDouble2x32", desc("Returns a two-component unsigned integer vector representation of v.")));
	idents.insert(std::make_pair("length", desc("Returns the length of the vector v.")));
	idents.insert(std::make_pair("distance", desc("Returns the distance between two points.")));
	idents.insert(std::make_pair("dot", desc("Returns the dot product of two vectors.")));
	idents.insert(std::make_pair("cross", desc("Returns the cross product of two 3D vectors.")));
	idents.insert(std::make_pair("normalize", desc("Returns a normalized vector.")));
	idents.insert(std::make_pair("faceforward", desc("Returns -n * sign(dot(i, ng)).")));
	idents.insert(std::make_pair("reflect", desc("Returns a reflection vector.")));
	idents.insert(std::make_pair("refract", desc("Returns the refraction vector.")));
	idents.insert(std::make_pair("matrixCompMult", desc("Multiply matrix x by matrix y component-wise.")));
	idents.insert(std::make_pair("outerProduct", desc("Linear algebraic matrix multiply c * r.")));
	idents.insert(std::make_pair("transpose", desc("Returns the transpose of the matrix m.")));
	idents.insert(std::make_pair("determinant", desc("Returns the determinant of the square matrix m.")));
	idents.insert(std::make_pair("inverse", desc("Returns a matrix that is the inverse of m.")));
	idents.insert(std::make_pair("lessThan", desc("Returns the component-wise compare of x < y")));
	idents.insert(std::make_pair("lessThanEqual", desc("Returns the component-wise compare of x <= y")));
	idents.insert(std::make_pair("greaterThan", desc("Returns the component-wise compare of x > y")));
	idents.insert(std::make_pair("greaterThanEqual", desc("Returns the component-wise compare of x >= y")));
	idents.insert(std::make_pair("equal", desc("Returns the component-wise compare of x == y")));
	idents.insert(std::make_pair("notEqual", desc("Returns the component-wise compare of x != y")));
	idents.insert(std::make_pair("any", desc("Test if any component of x is nonzero.")));
	idents.insert(std::make_pair("all", desc("Test if all components of x are nonzero.")));
	idents.insert(std::make_pair("not", desc("Returns the component-wise logical complement of x.")));
	idents.insert(std::make_pair("uaddCarry", desc("Adds 32bit unsigned integer x and y, returning the sum modulo 2^32.")));
	idents.insert(std::make_pair("usubBorrow", desc("Subtracts the 32bit unsigned integer y from x, returning the difference if non-negatice, or 2^32 plus the difference otherwise.")));
	idents.insert(std::make_pair("umulExtended", desc("Multiplies 32bit integers x and y, producing a 64bit result.")));
	idents.insert(std::make_pair("imulExtended", desc("Multiplies 32bit integers x and y, producing a 64bit result.")));
	idents.insert(std::make_pair("bitfieldExtract", desc("Extracts bits [offset, offset + bits - 1] from value, returning them in the least significant bits of the result.")));
	idents.insert(std::make_pair("bitfieldInsert", desc("Returns the insertion the bits leas-significant bits of insert into base")));
	idents.insert(std::make_pair("bitfieldReverse", desc("Returns the reversal of the bits of value.")));
	idents.insert(std::make_pair("bitCount", desc("Returns the number of bits set to 1 in the binary representation of value.")));
	idents.insert(std::make_pair("findLSB", desc("Returns the bit number of the least significant bit set to 1 in the binary representation of value.")));
	idents.insert(std::make_pair("findMSB", desc("Returns the bit number of the most significant bit in the binary representation of value.")));
	idents.insert(std::make_pair("textureSize", desc("Returns the dimensions of level lod  (if present) for the texture bound to sample.")));
	idents.insert(std::make_pair("textureQueryLod", desc("Returns the mipmap array(s) that would be accessed in the x component of the return value.")));
	idents.insert(std::make_pair("texture", desc("Use the texture coordinate P to do a texture lookup in the texture currently bound to sampler.")));
	idents.insert(std::make_pair("textureProj", desc("Do a texture lookup with projection.")));
	idents.insert(std::make_pair("textureLod", desc("Do a texture lookup as in texture but with explicit LOD.")));
	idents.insert(std::make_pair("textureOffset", desc("Do a texture lookup as in texture but with offset added to the (u,v,w) texel coordinates before looking up each texel.")));
	idents.insert(std::make_pair("texelFetch", desc("Use integer texture coordinate P to lookup a single texel from sampler.")));
	idents.insert(std::make_pair("texelFetchOffset", desc("Fetch a single texel as in texelFetch offset by offset.")));
	idents.insert(std::make_pair("texetureProjOffset", desc("Do a projective texture lookup as described in textureProj offset by offset as descrived in textureOffset.")));
	idents.insert(std::make_pair("texetureLodOffset", desc("Do an offset texture lookup with explicit LOD.")));
	idents.insert(std::make_pair("textureProjLod", desc("Do a projective texture lookup with explicit LOD.")));
	idents.insert(std::make_pair("textureLodOffset", desc("Do an offset texture lookup with explicit LOD.")));
	idents.insert(std::make_pair("textureProjLodOffset", desc("Do an offset projective texture lookup with explicit LOD.")));
	idents.insert(std::make_pair("textureGrad", desc("Do a texture lookup as in texture but with explicit gradients.")));
	idents.insert(std::make_pair("textureGradOffset", desc("Do a texture lookup with both explicit gradient and offset, as described in textureGrad and textureOffset.")));
	idents.insert(std::make_pair("textureProjGrad", desc("Do a texture lookup both projectively and with explicit gradient.")));
	idents.insert(std::make_pair("textureProjGradOffset", desc("Do a texture lookup both projectively and with explicit gradient as well as with offset.")));
	idents.insert(std::make_pair("textureGather", desc("Built-in function.")));
	idents.insert(std::make_pair("textureGatherOffset", desc("Built-in function.")));
	idents.insert(std::make_pair("textureGatherOffsets", desc("Built-in function.")));
	idents.insert(std::make_pair("texture1D", desc("1D texture lookup.")));
	idents.insert(std::make_pair("texture1DLod", desc("1D texture lookup with LOD.")));
	idents.insert(std::make_pair("texture1DProj", desc("1D texture lookup with projective divide.")));
	idents.insert(std::make_pair("texture1DProjLod", desc("1D texture lookup with projective divide and with LOD.")));
	idents.insert(std::make_pair("texture2D", desc("2D texture lookup.")));
	idents.insert(std::make_pair("texture2DLod", desc("2D texture lookup with LOD.")));
	idents.insert(std::make_pair("texture2DProj", desc("2D texture lookup with projective divide.")));
	idents.insert(std::make_pair("texture2DProjLod", desc("2D texture lookup with projective divide and with LOD.")));
	idents.insert(std::make_pair("texture3D", desc("3D texture lookup.")));
	idents.insert(std::make_pair("texture3DLod", desc("3D texture lookup with LOD.")));
	idents.insert(std::make_pair("texture3DProj", desc("3D texture lookup with projective divide.")));
	idents.insert(std::make_pair("texture3DProjLod", desc("3D texture lookup with projective divide and with LOD.")));
	idents.insert(std::make_pair("textureCube", desc("Cube texture lookup.")));
	idents.insert(std::make_pair("textureCubeLod", desc("Cube texture lookup with LOD.")));
	idents.insert(std::make_pair("shadow1D", desc("1D texture lookup.")));
	idents.insert(std::make_pair("shadow1DLod", desc("1D texture lookup with LOD.")));
	idents.insert(std::make_pair("shadow1DProj", desc("1D texture lookup with projective divide.")));
	idents.insert(std::make_pair("shadow1DProjLod", desc("1D texture lookup with projective divide and with LOD.")));
	idents.insert(std::make_pair("shadow2D", desc("2D texture lookup.")));
	idents.insert(std::make_pair("shadow2DLod", desc("2D texture lookup with LOD.")));
	idents.insert(std::make_pair("shadow2DProj", desc("2D texture lookup with projective divide.")));
	idents.insert(std::make_pair("shadow2DProjLod", desc("2D texture lookup with projective divide and with LOD.")));
	idents.insert(std::make_pair("dFdx", desc("Returns the partial derivative of x with respect to the screen-space x-coordinate.")));
	idents.insert(std::make_pair("dFdy", desc("Returns the partial derivative of x with respect to the screen-space y-coordinate.")));
	idents.insert(std::make_pair("fwidth", desc("Returns abs(ddx(x)) + abs(ddy(x))")));
	idents.insert(std::make_pair("interpolateAtCentroid", desc("Return the value of the input varying interpolant sampled at a location inside the both the pixel and the primitive being processed.")));
	idents.insert(std::make_pair("interpolateAtSample", desc("Return the value of the input varying interpolant at the location of sample number sample.")));
	idents.insert(std::make_pair("interpolateAtOffset", desc("Return the value of the input varying interpolant sampled at an offset from the center of the pixel specified by offset.")));
	idents.insert(std::make_pair("noise1", desc("Generates a random value")));
	idents.insert(std::make_pair("noise2", desc("Generates a random value")));
	idents.insert(std::make_pair("noise3", desc("Generates a random value")));
	idents.insert(std::make_pair("noise4", desc("Generates a random value")));
	idents.insert(std::make_pair("EmitStreamVertex", desc("Emit the current values of output variables to the current output primitive on stream stream.")));
	idents.insert(std::make_pair("EndStreamPrimitive", desc("Completes the current output primitive on stream stream and starts a new one.")));
	idents.insert(std::make_pair("EmitVertex", desc("Emit the current values to the current output primitive.")));
	idents.insert(std::make_pair("EndPrimitive", desc("Completes the current output primitive and starts a new one.")));
	idents.insert(std::make_pair("barrier", desc("For any given static instance of barrier(), all tessellation control shader invocations for a single input patch must enter it before any will be allowed to continue beyond it.")));
}

const TextEditor::LanguageDefinition& TextEditor::LanguageDefinition::C()
{
	static bool inited = false;
	static LanguageDefinition langDef;
	if (!inited)
	{
		static const char* const keywords[] = {
			"auto", "break", "case", "char", "const", "continue", "default", "do", "double", "else", "enum", "extern", "float", "for", "goto", "if", "inline", "int", "long", "register", "restrict", "return", "short",
			"signed", "sizeof", "static", "struct", "switch", "typedef", "union", "unsigned", "void", "volatile", "while", "_Alignas", "_Alignof", "_Atomic", "_Bool", "_Complex", "_Generic", "_Imaginary",
			"_Noreturn", "_Static_assert", "_Thread_local"
		};
		for (auto& k : keywords)
			langDef.mKeywords.insert(k);

		static const char* const identifiers[] = {
			"abort", "abs", "acos", "asin", "atan", "atexit", "atof", "atoi", "atol", "ceil", "clock", "cosh", "ctime", "div", "exit", "fabs", "floor", "fmod", "getchar", "getenv", "isalnum", "isalpha", "isdigit", "isgraph",
			"ispunct", "isspace", "isupper", "kbhit", "log10", "log2", "log", "memcmp", "modf", "pow", "putchar", "putenv", "puts", "rand", "remove", "rename", "sinh", "sqrt", "srand", "strcat", "strcmp", "strerror", "time", "tolower", "toupper"
		};
		for (auto& k : identifiers)
		{
			Identifier id;
			id.mDeclaration = "Built-in function";
			langDef.mIdentifiers.insert(std::make_pair(std::string(k), id));
		}

		langDef.mTokenize = [](const char* in_begin, const char* in_end, const char*& out_begin, const char*& out_end, PaletteIndex & paletteIndex) -> bool
		{
			paletteIndex = PaletteIndex::Max;

			while (in_begin < in_end && isascii(*in_begin) && isblank(*in_begin))
				in_begin++;

			if (in_begin == in_end)
			{
				out_begin = in_end;
				out_end = in_end;
				paletteIndex = PaletteIndex::Default;
			}
			else if (TokenizeCStyleString(in_begin, in_end, out_begin, out_end))
				paletteIndex = PaletteIndex::String;
			else if (TokenizeCStyleCharacterLiteral(in_begin, in_end, out_begin, out_end))
				paletteIndex = PaletteIndex::CharLiteral;
			else if (TokenizeCStyleIdentifier(in_begin, in_end, out_begin, out_end))
				paletteIndex = PaletteIndex::Identifier;
			else if (TokenizeCStyleNumber(in_begin, in_end, out_begin, out_end))
				paletteIndex = PaletteIndex::Number;
			else if (TokenizeCStylePunctuation(in_begin, in_end, out_begin, out_end))
				paletteIndex = PaletteIndex::Punctuation;

			return paletteIndex != PaletteIndex::Max;
		};

		langDef.mCommentStart = "/*";
		langDef.mCommentEnd = "*/";
		langDef.mSingleLineComment = "//";
		
		langDef.mCaseSensitive = true;
		langDef.mAutoIndentation = true;

		langDef.mName = "C";

		inited = true;
	}
	return langDef;
}

const TextEditor::LanguageDefinition& TextEditor::LanguageDefinition::SQL()
{
	static bool inited = false;
	static LanguageDefinition langDef;
	if (!inited)
	{
		static const char* const keywords[] = {
			"ADD", "EXCEPT", "PERCENT", "ALL", "EXEC", "PLAN", "ALTER", "EXECUTE", "PRECISION", "AND", "EXISTS", "PRIMARY", "ANY", "EXIT", "PRINT", "AS", "FETCH", "PROC", "ASC", "FILE", "PROCEDURE",
			"AUTHORIZATION", "FILLFACTOR", "PUBLIC", "BACKUP", "FOR", "RAISERROR", "BEGIN", "FOREIGN", "READ", "BETWEEN", "FREETEXT", "READTEXT", "BREAK", "FREETEXTTABLE", "RECONFIGURE",
			"BROWSE", "FROM", "REFERENCES", "BULK", "FULL", "REPLICATION", "BY", "FUNCTION", "RESTORE", "CASCADE", "GOTO", "RESTRICT", "CASE", "GRANT", "RETURN", "CHECK", "GROUP", "REVOKE",
			"CHECKPOINT", "HAVING", "RIGHT", "CLOSE", "HOLDLOCK", "ROLLBACK", "CLUSTERED", "IDENTITY", "ROWCOUNT", "COALESCE", "IDENTITY_INSERT", "ROWGUIDCOL", "COLLATE", "IDENTITYCOL", "RULE",
			"COLUMN", "IF", "SAVE", "COMMIT", "IN", "SCHEMA", "COMPUTE", "INDEX", "SELECT", "CONSTRAINT", "INNER", "SESSION_USER", "CONTAINS", "INSERT", "SET", "CONTAINSTABLE", "INTERSECT", "SETUSER",
			"CONTINUE", "INTO", "SHUTDOWN", "CONVERT", "IS", "SOME", "CREATE", "JOIN", "STATISTICS", "CROSS", "KEY", "SYSTEM_USER", "CURRENT", "KILL", "TABLE", "CURRENT_DATE", "LEFT", "TEXTSIZE",
			"CURRENT_TIME", "LIKE", "THEN", "CURRENT_TIMESTAMP", "LINENO", "TO", "CURRENT_USER", "LOAD", "TOP", "CURSOR", "NATIONAL", "TRAN", "DATABASE", "NOCHECK", "TRANSACTION",
			"DBCC", "NONCLUSTERED", "TRIGGER", "DEALLOCATE", "NOT", "TRUNCATE", "DECLARE", "NULL", "TSEQUAL", "DEFAULT", "NULLIF", "UNION", "DELETE", "OF", "UNIQUE", "DENY", "OFF", "UPDATE",
			"DESC", "OFFSETS", "UPDATETEXT", "DISK", "ON", "USE", "DISTINCT", "OPEN", "USER", "DISTRIBUTED", "OPENDATASOURCE", "VALUES", "DOUBLE", "OPENQUERY", "VARYING","DROP", "OPENROWSET", "VIEW",
			"DUMMY", "OPENXML", "WAITFOR", "DUMP", "OPTION", "WHEN", "ELSE", "OR", "WHERE", "END", "ORDER", "WHILE", "ERRLVL", "OUTER", "WITH", "ESCAPE", "OVER", "WRITETEXT"
		};

		for (auto& k : keywords)
			langDef.mKeywords.insert(k);

		static const char* const identifiers[] = {
			"ABS",  "ACOS",  "ADD_MONTHS",  "ASCII",  "ASCIISTR",  "ASIN",  "ATAN",  "ATAN2",  "AVG",  "BFILENAME",  "BIN_TO_NUM",  "BITAND",  "CARDINALITY",  "CASE",  "CAST",  "CEIL",
			"CHARTOROWID",  "CHR",  "COALESCE",  "COMPOSE",  "CONCAT",  "CONVERT",  "CORR",  "COS",  "COSH",  "COUNT",  "COVAR_POP",  "COVAR_SAMP",  "CUME_DIST",  "CURRENT_DATE",
			"CURRENT_TIMESTAMP",  "DBTIMEZONE",  "DECODE",  "DECOMPOSE",  "DENSE_RANK",  "DUMP",  "EMPTY_BLOB",  "EMPTY_CLOB",  "EXP",  "EXTRACT",  "FIRST_VALUE",  "FLOOR",  "FROM_TZ",  "GREATEST",
			"GROUP_ID",  "HEXTORAW",  "INITCAP",  "INSTR",  "INSTR2",  "INSTR4",  "INSTRB",  "INSTRC",  "LAG",  "LAST_DAY",  "LAST_VALUE",  "LEAD",  "LEAST",  "LENGTH",  "LENGTH2",  "LENGTH4",
			"LENGTHB",  "LENGTHC",  "LISTAGG",  "LN",  "LNNVL",  "LOCALTIMESTAMP",  "LOG",  "LOWER",  "LPAD",  "LTRIM",  "MAX",  "MEDIAN",  "MIN",  "MOD",  "MONTHS_BETWEEN",  "NANVL",  "NCHR",
			"NEW_TIME",  "NEXT_DAY",  "NTH_VALUE",  "NULLIF",  "NUMTODSINTERVAL",  "NUMTOYMINTERVAL",  "NVL",  "NVL2",  "POWER",  "RANK",  "RAWTOHEX",  "REGEXP_COUNT",  "REGEXP_INSTR",
			"REGEXP_REPLACE",  "REGEXP_SUBSTR",  "REMAINDER",  "REPLACE",  "ROUND",  "ROWNUM",  "RPAD",  "RTRIM",  "SESSIONTIMEZONE",  "SIGN",  "SIN",  "SINH",
			"SOUNDEX",  "SQRT",  "STDDEV",  "SUBSTR",  "SUM",  "SYS_CONTEXT",  "SYSDATE",  "SYSTIMESTAMP",  "TAN",  "TANH",  "TO_CHAR",  "TO_CLOB",  "TO_DATE",  "TO_DSINTERVAL",  "TO_LOB",
			"TO_MULTI_BYTE",  "TO_NCLOB",  "TO_NUMBER",  "TO_SINGLE_BYTE",  "TO_TIMESTAMP",  "TO_TIMESTAMP_TZ",  "TO_YMINTERVAL",  "TRANSLATE",  "TRIM",  "TRUNC", "TZ_OFFSET",  "UID",  "UPPER",
			"USER",  "USERENV",  "VAR_POP",  "VAR_SAMP",  "VARIANCE",  "VSIZE "
		};
		for (auto& k : identifiers)
		{
			Identifier id;
			id.mDeclaration = "Built-in function";
			langDef.mIdentifiers.insert(std::make_pair(std::string(k), id));
		}

		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::String));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("\\\'[^\\\']*\\\'", PaletteIndex::String));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/\\;\\,\\.]", PaletteIndex::Punctuation));

		langDef.mCommentStart = "/*";
		langDef.mCommentEnd = "*/";
		langDef.mSingleLineComment = "//";

		langDef.mCaseSensitive = false;
		langDef.mAutoIndentation = false;

		langDef.mName = "SQL";

		inited = true;
	}
	return langDef;
}

const TextEditor::LanguageDefinition& TextEditor::LanguageDefinition::AngelScript()
{
	static bool inited = false;
	static LanguageDefinition langDef;
	if (!inited)
	{
		static const char* const keywords[] = {
			"and", "abstract", "auto", "bool", "break", "case", "cast", "class", "const", "continue", "default", "do", "double", "else", "enum", "false", "final", "float", "for",
			"from", "funcdef", "function", "get", "if", "import", "in", "inout", "int", "interface", "int8", "int16", "int32", "int64", "is", "mixin", "namespace", "not",
			"null", "or", "out", "override", "private", "protected", "return", "set", "shared", "super", "switch", "this ", "true", "typedef", "uint", "uint8", "uint16", "uint32",
			"uint64", "void", "while", "xor"
		};

		for (auto& k : keywords)
			langDef.mKeywords.insert(k);

		static const char* const identifiers[] = {
			"cos", "sin", "tab", "acos", "asin", "atan", "atan2", "cosh", "sinh", "tanh", "log", "log10", "pow", "sqrt", "abs", "ceil", "floor", "fraction", "closeTo", "fpFromIEEE", "fpToIEEE",
			"complex", "opEquals", "opAddAssign", "opSubAssign", "opMulAssign", "opDivAssign", "opAdd", "opSub", "opMul", "opDiv"
		};
		for (auto& k : identifiers)
		{
			Identifier id;
			id.mDeclaration = "Built-in function";
			langDef.mIdentifiers.insert(std::make_pair(std::string(k), id));
		}

		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::String));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("\\'\\\\?[^\\']\\'", PaletteIndex::String));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/\\;\\,\\.]", PaletteIndex::Punctuation));

		langDef.mCommentStart = "/*";
		langDef.mCommentEnd = "*/";
		langDef.mSingleLineComment = "//";

		langDef.mCaseSensitive = true;
		langDef.mAutoIndentation = true;

		langDef.mName = "AngelScript";

		inited = true;
	}
	return langDef;
}

const TextEditor::LanguageDefinition& TextEditor::LanguageDefinition::Lua()
{
	static bool inited = false;
	static LanguageDefinition langDef;
	if (!inited)
	{
		static const char* const keywords[] = {
			"and", "break", "do", "", "else", "elseif", "end", "false", "for", "function", "if", "in", "", "local", "nil", "not", "or", "repeat", "return", "then", "true", "until", "while"
		};

		for (auto& k : keywords)
			langDef.mKeywords.insert(k);

		static const char* const identifiers[] = {
			"assert", "collectgarbage", "dofile", "error", "getmetatable", "ipairs", "loadfile", "load", "loadstring",  "next",  "pairs",  "pcall",  "print",  "rawequal",  "rawlen",  "rawget",  "rawset",
			"select",  "setmetatable",  "tonumber",  "tostring",  "type",  "xpcall",  "_G",  "_VERSION","arshift", "band", "bnot", "bor", "bxor", "btest", "extract", "lrotate", "lshift", "replace", 
			"rrotate", "rshift", "create", "resume", "running", "status", "wrap", "yield", "isyieldable", "debug","getuservalue", "gethook", "getinfo", "getlocal", "getregistry", "getmetatable", 
			"getupvalue", "upvaluejoin", "upvalueid", "setuservalue", "sethook", "setlocal", "setmetatable", "setupvalue", "traceback", "close", "flush", "input", "lines", "open", "output", "popen", 
			"read", "tmpfile", "type", "write", "close", "flush", "lines", "read", "seek", "setvbuf", "write", "__gc", "__tostring", "abs", "acos", "asin", "atan", "ceil", "cos", "deg", "exp", "tointeger",
			"floor", "fmod", "ult", "log", "max", "min", "modf", "rad", "random", "randomseed", "sin", "sqrt", "string", "tan", "type", "atan2", "cosh", "sinh", "tanh",
			 "pow", "frexp", "ldexp", "log10", "pi", "huge", "maxinteger", "mininteger", "loadlib", "searchpath", "seeall", "preload", "cpath", "path", "searchers", "loaded", "module", "require", "clock",
			 "date", "difftime", "execute", "exit", "getenv", "remove", "rename", "setlocale", "time", "tmpname", "byte", "char", "dump", "find", "format", "gmatch", "gsub", "len", "lower", "match", "rep",
			 "reverse", "sub", "upper", "pack", "packsize", "unpack", "concat", "maxn", "insert", "pack", "unpack", "remove", "move", "sort", "offset", "codepoint", "char", "len", "codes", "charpattern",
			 "coroutine", "table", "io", "os", "string", "utf8", "bit32", "math", "debug", "package"
		};
		for (auto& k : identifiers)
		{
			Identifier id;
			id.mDeclaration = "Built-in function";
			langDef.mIdentifiers.insert(std::make_pair(std::string(k), id));
		}

		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::String));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("\\\'[^\\\']*\\\'", PaletteIndex::String));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/\\;\\,\\.]", PaletteIndex::Punctuation));

		langDef.mCommentStart = "--[[";
		langDef.mCommentEnd = "]]";
		langDef.mSingleLineComment = "--";

		langDef.mCaseSensitive = true;
		langDef.mAutoIndentation = false;

		langDef.mName = "Lua";

		inited = true;
	}
	return langDef;
}