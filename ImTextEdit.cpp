////	#include <SDL2/SDL_events.h>
////	#include <SDL2/SDL_keyboard.h>
#include <algorithm>
#include <functional>
#include <chrono>
#include <string>
#include <regex>
#include <cmath>
#include <stack>

#include "ImTextEdit.h"

#ifdef max
	#undef max
#endif
#ifdef min
	#undef min
#endif

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

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

int isBracket(char ch) {
	if (ch == '(' || ch == '[' || ch == '{')
		return 1;
	if (ch == ')' || ch == ']' || ch == '}')
		return 2;
	return 0;
}
bool isClosingBracket(char open, char actual)
{
	return (open == '{' && actual == '}') || (open == '[' && actual == ']') || (open == '(' && actual == ')');
}
bool isOpeningBracket(char close, char actual)
{
	return (close == '}' && actual == '{') || (close == ']' && actual == '[') || (close == ')' && actual == '(');
}

ImTextEdit::ImTextEdit()
	: mLineSpacing(1.0f)
	, mUndoIndex(0)
	, mInsertSpaces(false)
	, mTabSize(4)
	, mHighlightBrackets(false)
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
	, mLeftMargin(DebugDataSpace + LineNumberSpace)
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
	, mDebugBar(false)
	, mDebugCurrentLineUpdated(false)
	, mDebugCurrentLine(-1)
	, mPath("")
	, OnContentUpdate(nullptr)
	, mFuncTooltips(true)
	, mUIScale(1.0f)
	, mUIFontSize(18.0f)
	, mEditorFontSize(18.0f)
	, mActiveAutocomplete(false)
	, m_readyForAutocomplete(false)
	, m_requestAutocomplete(false)
	, mScrollbarMarkers(false)
	, mAutoindentOnPaste(false)
	, mFunctionDeclarationTooltip(false)
	, mFunctionDeclarationTooltipEnabled(false)
	, mIsSnippet(false)
	, mSnippetTagSelected(0)
	, mSidebar(true)
	, mHasSearch(true)
	, mReplaceIndex(0)
	, mFoldEnabled(true)
	, mFoldLastIteration(0)
	, mFoldSorted(false)
	, mLastScroll(0.0f)
	, mStartTime(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
{
	memset(mFindWord, 0, 256 * sizeof(char));
	memset(mReplaceWord, 0, 256 * sizeof(char));
	mFindOpened = false;
	mReplaceOpened = false;
	mFindJustOpened = false;
	mFindFocused = false;
	mReplaceFocused = false;

	OnDebuggerJump = nullptr;
	OnDebuggerAction = nullptr;
	OnBreakpointRemove = nullptr;
	OnBreakpointUpdate = nullptr;
	OnIdentifierHover = nullptr;
	HasIdentifierHover = nullptr;
	OnExpressionHover = nullptr;
	HasExpressionHover = nullptr;

	RequestOpen = nullptr;

	mDebugBarWidth = 0.0f;
	mDebugBarHeight = 0.0f;

	SetPalette(GetDarkPalette());
	SetLanguageDefinition(LanguageDefinition::HLSL());
	mLines.push_back(Line());

	m_shortcuts = GetDefaultShortcuts();
}

const std::vector<ImTextEdit::Shortcut> ImTextEdit::GetDefaultShortcuts()
{
	std::vector<ImTextEdit::Shortcut> ret;
	ret.resize((int)ImTextEdit::ShortcutID::Count);

	ret[(int)ImTextEdit::ShortcutID::Undo] = ImTextEdit::Shortcut(SDLK_z, -1, 0, 1, 0); // CTRL+Z
	ret[(int)ImTextEdit::ShortcutID::Redo] = ImTextEdit::Shortcut(SDLK_y, -1, 0, 1, 0); // CTRL+Y
	ret[(int)ImTextEdit::ShortcutID::MoveUp] = ImTextEdit::Shortcut(SDLK_UP, -1, 0, 0, 0); // UP ARROW
	ret[(int)ImTextEdit::ShortcutID::SelectUp] = ImTextEdit::Shortcut(SDLK_UP, -1, 0, 0, 1); // SHIFT + UP ARROW
	ret[(int)ImTextEdit::ShortcutID::MoveDown] = ImTextEdit::Shortcut(SDLK_DOWN, -1, 0, 0, 0); // DOWN ARROW
	ret[(int)ImTextEdit::ShortcutID::SelectDown] = ImTextEdit::Shortcut(SDLK_DOWN, -1, 0, 0, 1); // SHIFT + DOWN ARROW
	ret[(int)ImTextEdit::ShortcutID::MoveLeft] = ImTextEdit::Shortcut(SDLK_LEFT, -1, 0, 0, 0); // LEFT ARROW (+ SHIFT/CTRL)
	ret[(int)ImTextEdit::ShortcutID::SelectLeft] = ImTextEdit::Shortcut(SDLK_LEFT, -1, 0, 0, 1); // SHIFT + LEFT ARROW
	ret[(int)ImTextEdit::ShortcutID::MoveWordLeft] = ImTextEdit::Shortcut(SDLK_LEFT, -1, 0, 1, 0); // CTRL + LEFT ARROW
	ret[(int)ImTextEdit::ShortcutID::SelectWordLeft] = ImTextEdit::Shortcut(SDLK_LEFT, -1, 0, 1, 1); // CTRL + SHIFT + LEFT ARROW
	ret[(int)ImTextEdit::ShortcutID::MoveRight] = ImTextEdit::Shortcut(SDLK_RIGHT, -1, 0, 0, 0); // RIGHT ARROW
	ret[(int)ImTextEdit::ShortcutID::SelectRight] = ImTextEdit::Shortcut(SDLK_RIGHT, -1, 0, 0, 1); // SHIFT + RIGHT ARROW
	ret[(int)ImTextEdit::ShortcutID::MoveWordRight] = ImTextEdit::Shortcut(SDLK_RIGHT, -1, 0, 1, 0); // CTRL + RIGHT ARROW
	ret[(int)ImTextEdit::ShortcutID::SelectWordRight] = ImTextEdit::Shortcut(SDLK_RIGHT, -1, 0, 1, 1); // CTRL + SHIFT + RIGHT ARROW
	ret[(int)ImTextEdit::ShortcutID::MoveUpBlock] = ImTextEdit::Shortcut(SDLK_PAGEUP, -1, 0, 0, 0); // PAGE UP
	ret[(int)ImTextEdit::ShortcutID::SelectUpBlock] = ImTextEdit::Shortcut(SDLK_PAGEUP, -1, 0, 0, 1); // SHIFT + PAGE UP
	ret[(int)ImTextEdit::ShortcutID::MoveDownBlock] = ImTextEdit::Shortcut(SDLK_PAGEDOWN, -1, 0, 0, 0); // PAGE DOWN
	ret[(int)ImTextEdit::ShortcutID::SelectDownBlock] = ImTextEdit::Shortcut(SDLK_PAGEDOWN, -1, 0, 0, 1); // SHIFT + PAGE DOWN
	ret[(int)ImTextEdit::ShortcutID::MoveTop] = ImTextEdit::Shortcut(SDLK_HOME, -1, 0, 1, 0); // CTRL + HOME
	ret[(int)ImTextEdit::ShortcutID::SelectTop] = ImTextEdit::Shortcut(SDLK_HOME, -1, 0, 1, 1); // CTRL + SHIFT + HOME
	ret[(int)ImTextEdit::ShortcutID::MoveBottom] = ImTextEdit::Shortcut(SDLK_END, -1, 0, 1, 0); // CTRL + END
	ret[(int)ImTextEdit::ShortcutID::SelectBottom] = ImTextEdit::Shortcut(SDLK_END, -1, 0, 1, 1); // CTRL + SHIFT + END
	ret[(int)ImTextEdit::ShortcutID::MoveStartLine] = ImTextEdit::Shortcut(SDLK_HOME, -1, 0, 0, 0); // HOME
	ret[(int)ImTextEdit::ShortcutID::SelectStartLine] = ImTextEdit::Shortcut(SDLK_HOME, -1, 0, 0, 1); // SHIFT + HOME
	ret[(int)ImTextEdit::ShortcutID::MoveEndLine] = ImTextEdit::Shortcut(SDLK_END, -1, 0, 0, 0); // END
	ret[(int)ImTextEdit::ShortcutID::SelectEndLine] = ImTextEdit::Shortcut(SDLK_END, -1, 0, 0, 1); // SHIFT + END
	ret[(int)ImTextEdit::ShortcutID::ForwardDelete] = ImTextEdit::Shortcut(SDLK_DELETE, -1, 0, 0, 0); // DELETE
	ret[(int)ImTextEdit::ShortcutID::ForwardDeleteWord] = ImTextEdit::Shortcut(SDLK_DELETE, -1, 0, 1, 0); // CTRL + DELETE
	ret[(int)ImTextEdit::ShortcutID::DeleteRight] = ImTextEdit::Shortcut(SDLK_DELETE, -1, 0, 0, 1); // SHIFT+BACKSPACE
	ret[(int)ImTextEdit::ShortcutID::BackwardDelete] = ImTextEdit::Shortcut(SDLK_BACKSPACE, -1, 0, 0, 0); // BACKSPACE
	ret[(int)ImTextEdit::ShortcutID::BackwardDeleteWord] = ImTextEdit::Shortcut(SDLK_BACKSPACE, -1, 0, 1, 0); // CTRL + BACKSPACE
	ret[(int)ImTextEdit::ShortcutID::DeleteLeft] = ImTextEdit::Shortcut(SDLK_BACKSPACE, -1, 0, 0, 1); // SHIFT+BACKSPACE
	ret[(int)ImTextEdit::ShortcutID::OverwriteCursor] = ImTextEdit::Shortcut(SDLK_INSERT, -1, 0, 0, 0); // INSERT
	ret[(int)ImTextEdit::ShortcutID::Copy] = ImTextEdit::Shortcut(SDLK_c, -1, 0, 1, 0); // CTRL+C
	ret[(int)ImTextEdit::ShortcutID::Paste] = ImTextEdit::Shortcut(SDLK_v, -1, 0, 1, 0); // CTRL+V
	ret[(int)ImTextEdit::ShortcutID::Cut] = ImTextEdit::Shortcut(SDLK_x, -1, 0, 1, 0); // CTRL+X
	ret[(int)ImTextEdit::ShortcutID::SelectAll] = ImTextEdit::Shortcut(SDLK_a, -1, 0, 1, 0); // CTRL+A
	ret[(int)ImTextEdit::ShortcutID::AutocompleteOpen] = ImTextEdit::Shortcut(SDLK_SPACE, -1, 0, 1, 0); // CTRL+SPACE
	ret[(int)ImTextEdit::ShortcutID::AutocompleteSelect] = ImTextEdit::Shortcut(SDLK_TAB, -1, 0, 0, 0); // TAB
	ret[(int)ImTextEdit::ShortcutID::AutocompleteSelectActive] = ImTextEdit::Shortcut(SDLK_RETURN, -1, 0, 0, 0); // RETURN
	ret[(int)ImTextEdit::ShortcutID::AutocompleteUp] = ImTextEdit::Shortcut(SDLK_UP, -1, 0, 0, 0); // UP ARROW
	ret[(int)ImTextEdit::ShortcutID::AutocompleteDown] = ImTextEdit::Shortcut(SDLK_DOWN, -1, 0, 0, 0); // DOWN ARROW
	ret[(int)ImTextEdit::ShortcutID::NewLine] = ImTextEdit::Shortcut(SDLK_RETURN, -1, 0, 0, 0); // RETURN
	ret[(int)ImTextEdit::ShortcutID::Indent] = ImTextEdit::Shortcut(SDLK_TAB, -1, 0, 0, 0); // TAB
	ret[(int)ImTextEdit::ShortcutID::Unindent] = ImTextEdit::Shortcut(SDLK_TAB, -1, 0, 0, 1); // SHIFT + TAB
	ret[(int)ImTextEdit::ShortcutID::Find] = ImTextEdit::Shortcut(SDLK_f, -1, 0, 1, 0); // CTRL+F
	ret[(int)ImTextEdit::ShortcutID::Replace] = ImTextEdit::Shortcut(SDLK_h, -1, 0, 1, 0); // CTRL+H
	ret[(int)ImTextEdit::ShortcutID::FindNext] = ImTextEdit::Shortcut(SDLK_F3, -1, 0, 0, 0); // F3
	ret[(int)ImTextEdit::ShortcutID::DebugStep] = ImTextEdit::Shortcut(SDLK_F10, -1, 0, 0, 0); // F10
	ret[(int)ImTextEdit::ShortcutID::DebugStepInto] = ImTextEdit::Shortcut(SDLK_F11, -1, 0, 0, 0); // F11
	ret[(int)ImTextEdit::ShortcutID::DebugStepOut] = ImTextEdit::Shortcut(SDLK_F11, -1, 0, 0, 1); // SHIFT+F11
	ret[(int)ImTextEdit::ShortcutID::DebugContinue] = ImTextEdit::Shortcut(SDLK_F5, -1, 0, 0, 0); // F5
	ret[(int)ImTextEdit::ShortcutID::DebugStop] = ImTextEdit::Shortcut(SDLK_F5, -1, 0, 0, 1); // SHIFT+F5
	ret[(int)ImTextEdit::ShortcutID::DebugBreakpoint] = ImTextEdit::Shortcut(SDLK_F9, -1, 0, 0, 0); // F9
	ret[(int)ImTextEdit::ShortcutID::DebugJumpHere] = ImTextEdit::Shortcut(SDLK_h, -1, 1, 1, 0); // CTRL+ALT+H
	ret[(int)ImTextEdit::ShortcutID::DuplicateLine] = ImTextEdit::Shortcut(SDLK_d, -1, 0, 1, 0);	// CTRL+D
	ret[(int)ImTextEdit::ShortcutID::CommentLines] = ImTextEdit::Shortcut(SDLK_k, -1, 0, 1, 1); // CTRL+SHIFT+K
	ret[(int)ImTextEdit::ShortcutID::UncommentLines] = ImTextEdit::Shortcut(SDLK_u, -1, 0, 1, 1); // CTRL+SHIFT+U

	return ret;
}

ImTextEdit::~ImTextEdit()
{
}

void ImTextEdit::SetLanguageDefinition(const LanguageDefinition & aLanguageDef)
{
	mLanguageDefinition = aLanguageDef;
	mRegexList.clear();

	for (auto& r : mLanguageDefinition.mTokenRegexStrings)
		mRegexList.push_back(std::make_pair(std::regex(r.first, std::regex_constants::optimize), r.second));

	Colorize();
}

void ImTextEdit::SetPalette(const Palette & aValue)
{
	mPaletteBase = aValue;
}

std::string ImTextEdit::GetText(const Coordinates & aStart, const Coordinates & aEnd) const
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
			if (!(lstart == lend-1 && iend == -1))
				result += '\n';
			++lstart;
		}
	}

	return result;
}

ImTextEdit::Coordinates ImTextEdit::GetActualCursorCoordinates() const
{
	return SanitizeCoordinates(mState.mCursorPosition);
}

ImTextEdit::Coordinates ImTextEdit::SanitizeCoordinates(const Coordinates & aValue) const
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
		column = std::max<int>(0, mLines.empty() ? 0 : std::min(column, GetLineMaxColumn(line)));
		return Coordinates(line, column);
	}
}

// https://en.wikipedia.org/wiki/UTF-8
// We assume that the char is a standalone character (<128) or a leading byte of an UTF-8 code sequence (non-10xxxxxx code)
static int UTF8CharLength(ImTextEdit::Char c)
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

void ImTextEdit::Advance(Coordinates & aCoordinates) const
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

void ImTextEdit::DeleteRange(const Coordinates& aStart, const Coordinates& aEnd)
{
	assert(aEnd >= aStart);
	assert(!mReadOnly);

	if (aEnd == aStart)
		return;

	auto start = GetCharacterIndex(aStart);
	auto end = GetCharacterIndex(aEnd);

	if (aStart.mLine == aEnd.mLine) {
		auto& line = mLines[aStart.mLine];
		auto n = GetLineMaxColumn(aStart.mLine);
		if (aEnd.mColumn >= n)
			line.erase(line.begin() + start, line.end());
		else
			line.erase(line.begin() + start, line.begin() + end);

		mRemoveFolds(aStart, aEnd);
	} else {
		auto& firstLine = mLines[aStart.mLine];
		auto& lastLine = mLines[aEnd.mLine];

		// remove the folds
		mRemoveFolds(aStart, aEnd);

		firstLine.erase(firstLine.begin() + start, firstLine.end());
		lastLine.erase(lastLine.begin(), lastLine.begin() + end);
		if (aStart.mLine < aEnd.mLine)
			firstLine.insert(firstLine.end(), lastLine.begin(), lastLine.end());

		if (aStart.mLine < aEnd.mLine) {
			// remove the actual lines
			RemoveLine(aStart.mLine + 1, aEnd.mLine + 1);
		}
	}

	if (mScrollbarMarkers) {
		for (int i = 0; i < mChangedLines.size(); i++) {
			if (mChangedLines[i] > aEnd.mLine)
				mChangedLines[i] -= (aEnd.mLine - aStart.mLine);
			else if (mChangedLines[i] > aStart.mLine && mChangedLines[i] < aEnd.mLine) {
				mChangedLines.erase(mChangedLines.begin() + i);
				i--;
			}
		}
	}

	mTextChanged = true;
	if (OnContentUpdate != nullptr)
		OnContentUpdate(this);
}

int ImTextEdit::InsertTextAt(Coordinates& /* inout */ aWhere, const char * aValue, bool indent)
{
	assert(!mReadOnly);

	int autoIndentStart = 0;
	for (int i = 0; i < mLines[aWhere.mLine].size() && indent; i++) {
		Char ch = mLines[aWhere.mLine][i].mChar;
		if (ch == ' ')
			autoIndentStart++;
		else if (ch == '\t')
			autoIndentStart += mTabSize;
		else break;
	}

	int cindex = GetCharacterIndex(aWhere);
	int totalLines = 0;
	int autoIndent = autoIndentStart;
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
			if (cindex < (int)mLines[aWhere.mLine].size() && cindex >= 0) {
				// normal stuff
				auto& newLine = InsertLine(aWhere.mLine + 1, aWhere.mColumn);
				auto& line = mLines[aWhere.mLine];
				newLine.insert(newLine.begin(), line.begin() + cindex, line.end());
				line.erase(line.begin() + cindex, line.end());

				
				// folding
				for (int b = 0; b < mFoldBegin.size(); b++)
					if (mFoldBegin[b].mLine == aWhere.mLine + 1 && mFoldBegin[b].mColumn >= aWhere.mColumn)
						mFoldBegin[b].mColumn = std::max<int>(0, mFoldBegin[b].mColumn - aWhere.mColumn);
				for (int b = 0; b < mFoldEnd.size(); b++)
					if (mFoldEnd[b].mLine == aWhere.mLine + 1 && mFoldEnd[b].mColumn >= aWhere.mColumn)
						mFoldEnd[b].mColumn = std::max<int>(0, mFoldEnd[b].mColumn - aWhere.mColumn);

			}
			else
			{
				InsertLine(aWhere.mLine + 1, aWhere.mColumn);
			}
			++aWhere.mLine;
			cindex = 0;
			aWhere.mColumn = 0;
			++totalLines;
			++aValue;

			if (indent) {
				bool lineIsAlreadyIndent = (isspace(*aValue) && *aValue != '\n' && *aValue != '\r');

				// first check if we need to "unindent"
				const char* bracketSearch = aValue;
				while (*bracketSearch != '\0' && isspace(*bracketSearch) && *bracketSearch != '\n')
					bracketSearch++;
				if (*bracketSearch == '}')
					autoIndent = std::max(0, autoIndent - mTabSize);

				int actualAutoIndent = autoIndent;
				if (lineIsAlreadyIndent) {
					actualAutoIndent = autoIndentStart;

					const char* aValueCopy = aValue;
					while (isspace(*aValueCopy) && *aValueCopy != '\n' && *aValueCopy != '\r' && *aValueCopy != 0) {
						actualAutoIndent = std::max(0, actualAutoIndent - mTabSize);
						aValueCopy++;
					}
				}

				// add tabs
				int tabCount = actualAutoIndent / mTabSize;
				int spaceCount = actualAutoIndent - tabCount * mTabSize;
				if (mInsertSpaces) {
					tabCount = 0;
					spaceCount = actualAutoIndent;
				}

				cindex = tabCount + spaceCount;
				aWhere.mColumn = actualAutoIndent;

				
				// folding
				for (int b = 0; b < mFoldBegin.size(); b++)
					if (mFoldBegin[b].mLine == aWhere.mLine && mFoldBegin[b].mColumn >= aWhere.mColumn)
						mFoldBegin[b].mColumn += spaceCount + tabCount * mTabSize;
				for (int b = 0; b < mFoldEnd.size(); b++)
					if (mFoldEnd[b].mLine == aWhere.mLine && mFoldEnd[b].mColumn >= aWhere.mColumn)
						mFoldEnd[b].mColumn += spaceCount + tabCount * mTabSize;

				// insert the spaces/tabs
				while (spaceCount-- > 0) {
					mLines[aWhere.mLine].insert(mLines[aWhere.mLine].begin(), Glyph(' ', PaletteIndex::Default));
					for (int i = 0; i < mSnippetTagStart.size(); i++) {
						if (mSnippetTagStart[i].mLine == aWhere.mLine) {
							mSnippetTagStart[i].mColumn++;
							mSnippetTagEnd[i].mColumn++;
						}
					}
				}
				while (tabCount-- > 0) {
					mLines[aWhere.mLine].insert(mLines[aWhere.mLine].begin(), Glyph('\t', PaletteIndex::Default));
					for (int i = 0; i < mSnippetTagStart.size(); i++) {
						if (mSnippetTagStart[i].mLine == aWhere.mLine) {
							mSnippetTagStart[i].mColumn += mTabSize;
							mSnippetTagEnd[i].mColumn += mTabSize;
						}
					}
				}
			}
		}
		else
		{
			char aValueOld = *aValue;
			bool isTab = (aValueOld == '\t');
			auto& line = mLines[aWhere.mLine];
			auto d = UTF8CharLength(aValueOld);
			int foldOffset = 0;
			while (d-- > 0 && *aValue != '\0') {
				foldOffset += (*aValue == '\t') ? mTabSize : 1;
				line.insert(line.begin() + cindex++, Glyph(*aValue++, PaletteIndex::Default));
			}

			// shift old fold info
			for (int i = 0; i < mFoldBegin.size(); i++)
				if (mFoldBegin[i].mLine == aWhere.mLine && mFoldBegin[i].mColumn >= aWhere.mColumn)
					mFoldBegin[i].mColumn += foldOffset;
			for (int i = 0; i < mFoldEnd.size(); i++)
				if (mFoldEnd[i].mLine == aWhere.mLine && mFoldEnd[i].mColumn >= aWhere.mColumn)
					mFoldEnd[i].mColumn += foldOffset;

			// insert new fold info
			if (aValueOld == '{') {
				autoIndent += mTabSize;

				mFoldBegin.push_back(aWhere);
				mFoldSorted = false;
			} else if (aValueOld == '}') {
				autoIndent = std::max(0, autoIndent - mTabSize);

				mFoldEnd.push_back(aWhere);
				mFoldSorted = false;
			}

			aWhere.mColumn += (isTab ? mTabSize : 1);
		}
	}

	if (mScrollbarMarkers) {
		bool changeExists = false;
		for (int i = 0; i < mChangedLines.size(); i++) {
			if (mChangedLines[i] == aWhere.mLine) {
				changeExists = true;
				break;
			}
		}
		if (!changeExists)
			mChangedLines.push_back(aWhere.mLine);
	}

	mTextChanged = true;
	if (OnContentUpdate != nullptr)
		OnContentUpdate(this);

	return totalLines;
}

void ImTextEdit::AddUndo(UndoRecord& aValue)
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

ImTextEdit::Coordinates ImTextEdit::ScreenPosToCoordinates(const ImVec2& aPosition) const
{
	ImVec2 origin = mUICursorPos;
	ImVec2 local(aPosition.x - origin.x, aPosition.y - origin.y);

	int lineNo = std::max(0, (int)floor(local.y / mCharAdvance.y));
	int columnCoord = 0;

	// check for folds
	if (mFoldEnabled) {
		auto foldLineStart = 0;
		auto foldLineEnd = std::min<int>((int)mLines.size() - 1, lineNo);
		while (foldLineStart < foldLineEnd) {
			// check if line is folded
			for (int i = 0; i < mFoldBegin.size(); i++) {
				if (mFoldBegin[i].mLine == foldLineStart) {
					if (i < mFold.size() && mFold[i]) {
						int foldCon = mFoldConnection[i];
						if (foldCon != -1 && foldCon < mFoldEnd.size()) {
							int diff = mFoldEnd[foldCon].mLine - mFoldBegin[i].mLine;
							lineNo += diff;
							foldLineEnd = std::min<int>((int)mLines.size() - 1, foldLineEnd + diff);
						}
						break;
					}
				}
			}
			foldLineStart++;
		}
	}

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
ImTextEdit::Coordinates ImTextEdit::MousePosToCoordinates(const ImVec2& aPosition) const
{
	ImVec2 origin = mUICursorPos;
	ImVec2 local(aPosition.x - origin.x, aPosition.y - origin.y);

	int lineNo = std::max(0, (int)floor(local.y / mCharAdvance.y));
	int columnCoord = 0;
	int modifier = 0;

	// check for folds
	if (mFoldEnabled) {
		auto foldLineStart = 0;
		auto foldLineEnd = std::min<int>((int)mLines.size() - 1, lineNo);
		while (foldLineStart < foldLineEnd) {
			// check if line is folded
			for (int i = 0; i < mFoldBegin.size(); i++) {
				if (mFoldBegin[i].mLine == foldLineStart) {
					if (i < mFold.size() && mFold[i]) {
						int foldCon = mFoldConnection[i];
						if (foldCon != -1 && foldCon < mFoldEnd.size()) {
							int diff = mFoldEnd[foldCon].mLine - mFoldBegin[i].mLine;
							lineNo += diff;
							foldLineEnd = std::min<int>((int)mLines.size() - 1, foldLineEnd + diff);
						}
						break;
					}
				}
			}
			foldLineStart++;
		}
	}

	if (lineNo >= 0 && lineNo < (int)mLines.size()) {
		auto& line = mLines.at(lineNo);

		int columnIndex = 0;
		float columnX = 0.0f;

		while ((size_t)columnIndex < line.size()) {
			float columnWidth = 0.0f;

			if (line[columnIndex].mChar == '\t') {
				float spaceSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, " ").x;
				float oldX = columnX;
				float newColumnX = (1.0f + std::floor((1.0f + columnX) / (float(mTabSize) * spaceSize))) * (float(mTabSize) * spaceSize);
				columnWidth = newColumnX - oldX;
				if (mTextStart + columnX + columnWidth * 0.5f > local.x)
					break;
				columnX = newColumnX;
				columnCoord = (columnCoord / mTabSize) * mTabSize + mTabSize;
				columnIndex++;
				modifier += 3;
			} else {
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

	return SanitizeCoordinates(Coordinates(lineNo, columnCoord - modifier));
}

ImTextEdit::Coordinates ImTextEdit::FindWordStart(const Coordinates & aFrom) const
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

ImTextEdit::Coordinates ImTextEdit::FindWordEnd(const Coordinates & aFrom) const
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

ImTextEdit::Coordinates ImTextEdit::FindNextWord(const Coordinates & aFrom) const
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

int ImTextEdit::GetCharacterIndex(const Coordinates& aCoordinates) const
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

int ImTextEdit::GetCharacterColumn(int aLine, int aIndex) const
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

int ImTextEdit::GetLineCharacterCount(int aLine) const
{
	if (aLine >= mLines.size())
		return 0;
	auto& line = mLines[aLine];
	int c = 0;
	for (unsigned i = 0; i < line.size(); c++)
		i += UTF8CharLength(line[i].mChar);
	return c;
}

int ImTextEdit::GetLineMaxColumn(int aLine) const
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

bool ImTextEdit::IsOnWordBoundary(const Coordinates & aAt) const
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

void ImTextEdit::RemoveLine(int aStart, int aEnd)
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

	auto btmp = mBreakpoints;
	mBreakpoints.clear();
	for (auto i : btmp)
	{
		if (i.mLine >= aStart && i.mLine <= aEnd) {
			RemoveBreakpoint(i.mLine);
			continue;
		}
		AddBreakpoint(i.mLine >= aStart ? i.mLine - 1 : i.mLine, i.mUseCondition, i.mCondition, i.mEnabled);
	}

	mLines.erase(mLines.begin() + aStart, mLines.begin() + aEnd);
	assert(!mLines.empty());

	// remove scrollbard markers
	if (mScrollbarMarkers) {
		for (int i = 0; i < mChangedLines.size(); i++) {
			if (mChangedLines[i] > aEnd)
				mChangedLines[i] -= (aEnd - aStart);
			else if (mChangedLines[i] >= aStart && mChangedLines[i] <= aEnd) {
				mChangedLines.erase(mChangedLines.begin() + i);
				i--;
			}
		}
	}

	mTextChanged = true;
	if (OnContentUpdate != nullptr)
		OnContentUpdate(this);
}

void ImTextEdit::RemoveLine(int aIndex)
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

	auto btmp = mBreakpoints;
	mBreakpoints.clear();
	for (auto i : btmp)
	{
		if (i.mLine == aIndex) {
			RemoveBreakpoint(i.mLine);
			continue;
		}
		AddBreakpoint(i.mLine >= aIndex ? i.mLine - 1 : i.mLine, i.mUseCondition, i.mCondition, i.mEnabled);
	}

	mLines.erase(mLines.begin() + aIndex);
	assert(!mLines.empty());
	
	// remove folds
	mRemoveFolds(Coordinates(aIndex, 0), Coordinates(aIndex, 100000));

	// move/remove scrollbar markers
	if (mScrollbarMarkers) {
		for (int i = 0; i < mChangedLines.size(); i++) {
			if (mChangedLines[i] > aIndex)
				mChangedLines[i]--;
			else if (mChangedLines[i] == aIndex) {
				mChangedLines.erase(mChangedLines.begin() + i);
				i--;
			}

		}
	}

	mTextChanged = true;
	if (OnContentUpdate != nullptr)
		OnContentUpdate(this);
}

ImTextEdit::Line& ImTextEdit::InsertLine(int aIndex, int column)
{
	assert(!mReadOnly);

	auto& result = *mLines.insert(mLines.begin() + aIndex, Line());

	// folding
	for (int b = 0; b < mFoldBegin.size(); b++) 
		if (mFoldBegin[b].mLine > aIndex - 1 || (mFoldBegin[b].mLine == aIndex - 1 && mFoldBegin[b].mColumn >= column))
			mFoldBegin[b].mLine++;
	for (int b = 0; b < mFoldEnd.size(); b++)
		if (mFoldEnd[b].mLine > aIndex - 1 || (mFoldEnd[b].mLine == aIndex - 1 && mFoldEnd[b].mColumn >= column))
			mFoldEnd[b].mLine++;

	// error markers
	ErrorMarkers etmp;
	for (auto& i : mErrorMarkers)
		etmp.insert(ErrorMarkers::value_type(i.first >= aIndex ? i.first + 1 : i.first, i.second));
	mErrorMarkers = std::move(etmp);

	// breakpoints
	auto btmp = mBreakpoints;
	mBreakpoints.clear();
	for (auto i : btmp)
		RemoveBreakpoint(i.mLine);
	for (auto i : btmp)
		AddBreakpoint(i.mLine >= aIndex ? i.mLine + 1 : i.mLine, i.mUseCondition, i.mCondition, i.mEnabled);

	return result;
}

std::string ImTextEdit::GetWordUnderCursor() const
{
	auto c = GetCursorPosition();
	c.mColumn = std::max(c.mColumn - 1, 0);
	return GetWordAt(c);
}

std::string ImTextEdit::GetWordAt(const Coordinates & aCoords) const
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

ImU32 ImTextEdit::GetGlyphColor(const Glyph & aGlyph) const
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

ImTextEdit::Coordinates ImTextEdit::FindFirst(const std::string& what, const Coordinates& fromWhere)
{
	if (fromWhere.mLine < 0 || fromWhere.mLine >= mLines.size())
		return Coordinates(mLines.size(), 0);

	std::string textSrc = GetText(fromWhere, Coordinates((int)mLines.size(), 0));

	size_t index = 0;
	size_t loc = textSrc.find(what);
	Coordinates ret = fromWhere;
	while (loc != std::string::npos) {
		for (; index < loc; index++) {
			if (textSrc[index] == '\n') {
				ret.mColumn = 0;
				ret.mLine++;
			} else
				ret.mColumn++;
		}
		ret.mColumn = GetCharacterColumn(ret.mLine, ret.mColumn); // ret.mColumn is currently character index, convert it to character column
		if (GetWordAt(ret) == what)
			return ret;
		else
			loc = textSrc.find(what, loc + 1);
	}

	return Coordinates(mLines.size(), 0);
}

void ImTextEdit::HandleKeyboardInputs()
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

			////	TODO: SDL keyboard stuff
			////	SDL_Scancode sc1 = SDL_GetScancodeFromKey(sct.Key1);
			////	
			////	if ((ImGui::IsKeyPressed(sc1) || (sc1 == SDL_SCANCODE_RETURN && ImGui::IsKeyPressed(SDL_SCANCODE_KP_ENTER))) && ((sct.Key2 != -1 && ImGui::IsKeyPressed(SDL_GetScancodeFromKey(sct.Key2))) || sct.Key2 == -1)) {
			if (false) {
				if ((sct.Ctrl == ctrl) && (sct.Alt == alt) && (sct.Shift == shift)) {

					// PRESSED:
					curActionID = (ImTextEdit::ShortcutID)i;
					switch (curActionID) {
						case ShortcutID::Paste:
						case ShortcutID::Cut:
						case ShortcutID::Redo: 
						case ShortcutID::Undo:
						case ShortcutID::ForwardDelete:
						case ShortcutID::BackwardDelete:
						case ShortcutID::DeleteLeft:
						case ShortcutID::DeleteRight:
						case ShortcutID::ForwardDeleteWord:
						case ShortcutID::BackwardDeleteWord:
							additionalChecks = !IsReadOnly();
						break;
						case ShortcutID::MoveUp:
						case ShortcutID::MoveDown:
						case ShortcutID::SelectUp:
						case ShortcutID::SelectDown:
							additionalChecks = !mACOpened;
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
						case ShortcutID::Indent:
						case ShortcutID::Unindent:
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
		bool keepACOpened = false, functionTooltipState = mFunctionDeclarationTooltip;
		bool hasWrittenALetter = false;
		if (actionID != ShortcutID::Count) {
			if (actionID != ShortcutID::Indent)
				mIsSnippet = false;
			
			switch (actionID) {
				case ShortcutID::Undo: Undo(); break;
				case ShortcutID::Redo: Redo(); break;
				case ShortcutID::MoveUp: MoveUp(1, false); break;
				case ShortcutID::SelectUp: MoveUp(1, true); break;
				case ShortcutID::MoveDown: MoveDown(1, false); break;
				case ShortcutID::SelectDown: MoveDown(1, true); break;
				case ShortcutID::MoveLeft: MoveLeft(1, false, false); break;
				case ShortcutID::SelectLeft: MoveLeft(1, true, false); break;
				case ShortcutID::MoveWordLeft: MoveLeft(1, false, true); break;
				case ShortcutID::SelectWordLeft: MoveLeft(1, true, true); break;
				case ShortcutID::MoveRight: MoveRight(1, false, false); break;
				case ShortcutID::SelectRight: MoveRight(1, true, false); break;
				case ShortcutID::MoveWordRight: MoveRight(1, false, true); break;
				case ShortcutID::SelectWordRight: MoveRight(1, true, true); break;
				case ShortcutID::MoveTop: MoveTop(false); break;
				case ShortcutID::SelectTop: MoveTop(true); break;
				case ShortcutID::MoveBottom: MoveBottom(false); break;
				case ShortcutID::SelectBottom: MoveBottom(true); break;
				case ShortcutID::MoveUpBlock: MoveUp(GetPageSize() - 4, false); break;
				case ShortcutID::MoveDownBlock: MoveDown(GetPageSize() - 4, false); break;
				case ShortcutID::SelectUpBlock: MoveUp(GetPageSize() - 4, true); break;
				case ShortcutID::SelectDownBlock: MoveDown(GetPageSize() - 4, true); break;
				case ShortcutID::MoveEndLine: MoveEnd(false); break;
				case ShortcutID::SelectEndLine: MoveEnd(true); break;
				case ShortcutID::MoveStartLine: MoveHome(false); break;
				case ShortcutID::SelectStartLine: MoveHome(true); break;
				case ShortcutID::DeleteRight:
				case ShortcutID::ForwardDelete: Delete(); break;
				case ShortcutID::ForwardDeleteWord:
					if (ctrl && mState.mSelectionStart == mState.mSelectionEnd)
						MoveRight(1, true, true);
					Delete();
				break;
				case ShortcutID::DeleteLeft:
				case ShortcutID::BackwardDelete: Backspace(); break;
				case ShortcutID::BackwardDeleteWord:
					if (ctrl && (mState.mSelectionStart == mState.mSelectionEnd || mState.mSelectionStart == mState.mCursorPosition))
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
					if (mAutocomplete && !mIsSnippet)
						m_buildSuggestions(&keepACOpened);
				} break;
				case ShortcutID::AutocompleteSelect: 
				case ShortcutID::AutocompleteSelectActive:
				{
					mAutocompleteSelect();
				}
				break;
				case ShortcutID::AutocompleteUp:
					mACIndex = std::max<int>(mACIndex - 1, 0), mACSwitched = true;
					keepACOpened = true;
				break;
				case ShortcutID::AutocompleteDown:
					mACIndex = std::min<int>(mACIndex + 1, (int)mACSuggestions.size()-1), mACSwitched = true;
					keepACOpened = true;
				break;
				case ShortcutID::NewLine:
					EnterCharacter('\n', false);
				break;
				case ShortcutID::Indent:
					if (mIsSnippet) {
						do {
							mSnippetTagSelected++;
							if (mSnippetTagSelected >= mSnippetTagStart.size())
								mSnippetTagSelected = 0;
						} while (!mSnippetTagHighlight[mSnippetTagSelected]);

						mSnippetTagLength = 0;
						mSnippetTagPreviousLength = mSnippetTagEnd[mSnippetTagSelected].mColumn - mSnippetTagStart[mSnippetTagSelected].mColumn;

						SetSelection(mSnippetTagStart[mSnippetTagSelected], mSnippetTagEnd[mSnippetTagSelected]);
						SetCursorPosition(mSnippetTagEnd[mSnippetTagSelected]);
					} else
						EnterCharacter('\t', false);
				break;
				case ShortcutID::Unindent:
					EnterCharacter('\t', true);
				break;
				case ShortcutID::Find: mFindOpened = mHasSearch; mFindJustOpened = mHasSearch; mReplaceOpened = false; break;
				case ShortcutID::Replace: mFindOpened = mHasSearch; mFindJustOpened = mHasSearch; mReplaceOpened = mHasSearch; break;
				case ShortcutID::DebugStep:
					if (OnDebuggerAction)
						OnDebuggerAction(this, ImTextEdit::DebugAction::Step);
				break;
				case ShortcutID::DebugStepInto:
					if (OnDebuggerAction)
						OnDebuggerAction(this, ImTextEdit::DebugAction::StepInto);
				break;
				case ShortcutID::DebugStepOut:
					if (OnDebuggerAction)
						OnDebuggerAction(this, ImTextEdit::DebugAction::StepOut);
				break;
				case ShortcutID::DebugContinue:
					if (OnDebuggerAction)
						OnDebuggerAction(this, ImTextEdit::DebugAction::Continue);
				break;
				case ShortcutID::DebugStop:
					if (OnDebuggerAction)
						OnDebuggerAction(this, ImTextEdit::DebugAction::Stop);
				break;
				case ShortcutID::DebugJumpHere:
					if (OnDebuggerJump)
						OnDebuggerJump(this, GetCursorPosition().mLine);
				break;
				case ShortcutID::DebugBreakpoint:
					if (OnBreakpointUpdate) {
						int line = GetCursorPosition().mLine + 1;
						if (HasBreakpoint(line))
							RemoveBreakpoint(line);
						else AddBreakpoint(line);
					}
				break;
				case ShortcutID::DuplicateLine: {
					ImTextEdit::UndoRecord undo;
					undo.mBefore = mState;

					auto oldLine = mLines[mState.mCursorPosition.mLine];
					auto& line = InsertLine(mState.mCursorPosition.mLine, mState.mCursorPosition.mColumn);

					undo.mAdded += '\n';
					for (auto& glyph : oldLine) {
						line.push_back(glyph);
						undo.mAdded += glyph.mChar;
					}
					mState.mCursorPosition.mLine++;

					undo.mAddedStart = ImTextEdit::Coordinates(mState.mCursorPosition.mLine-1, mState.mCursorPosition.mColumn);
					undo.mAddedEnd = mState.mCursorPosition;

					undo.mAfter = mState;

					AddUndo(undo);
				} break;
				case ShortcutID::CommentLines: {
					for (int l = mState.mSelectionStart.mLine; l <= mState.mSelectionEnd.mLine && l < mLines.size(); l++) {
						mLines[l].insert(mLines[l].begin(), ImTextEdit::Glyph('/', ImTextEdit::PaletteIndex::Comment));
						mLines[l].insert(mLines[l].begin(), ImTextEdit::Glyph('/', ImTextEdit::PaletteIndex::Comment));
					}
					Colorize(mState.mSelectionStart.mLine, mState.mSelectionEnd.mLine);
				} break;
				case ShortcutID::UncommentLines: {
					for (int l = mState.mSelectionStart.mLine; l <= mState.mSelectionEnd.mLine && l < mLines.size(); l++) {
						if (mLines[l].size() >= 2) {
							if (mLines[l][0].mChar == '/' && mLines[l][1].mChar == '/')
								mLines[l].erase(mLines[l].begin(), mLines[l].begin() + 2);
						}
					}
					Colorize(mState.mSelectionStart.mLine, mState.mSelectionEnd.mLine);
				} break;
			}
		} else if (!IsReadOnly()) {
			for (int i = 0; i < io.InputQueueCharacters.Size; i++)
			{
				auto c = (unsigned char)io.InputQueueCharacters[i];
				if (c != 0 && (c == '\n' || c >= 32)) {
					EnterCharacter((char)c, shift);

					if (c == '.')
						m_buildMemberSuggestions(&keepACOpened);

					if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
						hasWrittenALetter = true;

					if (mIsSnippet) {
						mSnippetTagLength++;
						mSnippetTagEnd[mSnippetTagSelected].mColumn = mSnippetTagStart[mSnippetTagSelected].mColumn + mSnippetTagLength;
					
						Coordinates curCursor = GetCursorPosition();
						SetSelection(mSnippetTagStart[mSnippetTagSelected], mSnippetTagEnd[mSnippetTagSelected]);
						std::string curWord = GetSelectedText();
						std::unordered_map<int, int> modif;
						modif[curCursor.mLine] = 0;
						for (int j = 0; j < mSnippetTagStart.size(); j++) {
							if (j != mSnippetTagSelected) {
								mSnippetTagStart[j].mColumn += modif[mSnippetTagStart[j].mLine];
								mSnippetTagEnd[j].mColumn += modif[mSnippetTagStart[j].mLine];
							}
							if (mSnippetTagID[j] == mSnippetTagID[mSnippetTagSelected]) {
								modif[mSnippetTagStart[j].mLine] += mSnippetTagLength - mSnippetTagPreviousLength;

								if (j != mSnippetTagSelected) {
									SetSelection(mSnippetTagStart[j], mSnippetTagEnd[j]);
									Backspace();
									InsertText(curWord);
									mSnippetTagEnd[j].mColumn = mSnippetTagStart[j].mColumn + mSnippetTagLength;
								}
							}
						}
						SetSelection(curCursor, curCursor);
						SetCursorPosition(curCursor);
						EnsureCursorVisible();
						mSnippetTagPreviousLength = mSnippetTagLength;
					}

					keyCount++;
				}
			}
			io.InputQueueCharacters.resize(0);
		}

		// active autocomplete
		if (m_requestAutocomplete && m_readyForAutocomplete && !mIsSnippet) {
			m_buildSuggestions(&keepACOpened);
			m_requestAutocomplete = false;
			m_readyForAutocomplete = false;
		}

		if ((mACOpened && !keepACOpened) || mFunctionDeclarationTooltip) {
			for (size_t i = 0; i < ImGuiKey_COUNT; i++)
				keyCount += ImGui::IsKeyPressed(ImGui::GetKeyIndex(i));

			if (keyCount != 0) {
				if (functionTooltipState == mFunctionDeclarationTooltip)
					mFunctionDeclarationTooltip = false;
				mACOpened = false;
				if (!hasWrittenALetter)
					mACObject = "";
			}
		}
	}
}

void ImTextEdit::HandleMouseInputs()
{
	ImGuiIO& io = ImGui::GetIO();
	auto shift = io.KeyShift;
	auto ctrl = io.ConfigMacOSXBehaviors ? io.KeySuper : io.KeyCtrl;
	auto alt = io.ConfigMacOSXBehaviors ? io.KeyCtrl : io.KeyAlt;

	if (ImGui::IsWindowHovered()) {
		auto click = ImGui::IsMouseClicked(0);
		if ((!shift || (shift && click)) && !alt)
		{
			auto doubleClick = ImGui::IsMouseDoubleClicked(0);
			auto t = ImGui::GetTime();
			auto tripleClick = click && !doubleClick && (mLastClick != -1.0f && (t - mLastClick) < io.MouseDoubleClickTime);

			if (click || doubleClick || tripleClick) {
				mIsSnippet = false;
				mFunctionDeclarationTooltip = false;
			}

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
					mState.mCursorPosition = mState.mSelectionEnd;
				}

				mLastClick = (float)ImGui::GetTime();
			}

			/*
				mouse button click
			*/
			else if (click)
			{
				ImVec2 pos = ImGui::GetMousePos();

				if (pos.x - mUICursorPos.x < ImGui::GetStyle().WindowPadding.x + mEditorCalculateSize(DebugDataSpace)) {
					Coordinates lineInfo = ScreenPosToCoordinates(ImGui::GetMousePos());
					lineInfo.mLine += 1;

					if (HasBreakpoint(lineInfo.mLine))
						RemoveBreakpoint(lineInfo.mLine);
					else AddBreakpoint(lineInfo.mLine);
				}
				else {
					mACOpened = false;
					mACObject = "";

					auto tcoords = ScreenPosToCoordinates(ImGui::GetMousePos());
					
					if (!shift)
						mInteractiveStart = tcoords;
					
					mState.mCursorPosition = mInteractiveEnd = tcoords;

					if (ctrl && !shift)
						mSelectionMode = SelectionMode::Word;
					else
						mSelectionMode = SelectionMode::Normal;
					SetSelection(mInteractiveStart, mInteractiveEnd, mSelectionMode);

					mLastClick = (float)ImGui::GetTime();
				}
			}
			// Mouse left button dragging (=> update selection)
			else if (ImGui::IsMouseDragging(0) && ImGui::IsMouseDown(0))
			{
				io.WantCaptureMouse = true;
				mState.mCursorPosition = mInteractiveEnd = ScreenPosToCoordinates(ImGui::GetMousePos());
				SetSelection(mInteractiveStart, mInteractiveEnd, mSelectionMode);

				float mx = ImGui::GetMousePos().x;
				if (mx > mFindOrigin.x + mWindowWidth - 50 && mx < mFindOrigin.x + mWindowWidth)
					ImGui::SetScrollX(ImGui::GetScrollX() + 1.0f);
				else if (mx > mFindOrigin.x && mx < mFindOrigin.x + mTextStart + 50)
					ImGui::SetScrollX(ImGui::GetScrollX() - 1.0f);
			}

		}
	}
}

bool ImTextEdit::HasBreakpoint(int line)
{
	for (const auto& bkpt : mBreakpoints)
		if (bkpt.mLine == line)
			return true;
	return false;
}
void ImTextEdit::AddBreakpoint(int line, bool useCondition, std::string condition, bool enabled)
{
	RemoveBreakpoint(line);

	Breakpoint bkpt;
	bkpt.mLine = line;
	bkpt.mCondition = condition;
	bkpt.mEnabled = enabled;
	bkpt.mUseCondition = useCondition;

	if (OnBreakpointUpdate)
		OnBreakpointUpdate(this, line, useCondition, condition, enabled);

	mBreakpoints.push_back(bkpt);
}
void ImTextEdit::RemoveBreakpoint(int line)
{
	for (int i = 0; i < mBreakpoints.size(); i++)
		if (mBreakpoints[i].mLine == line) {
			mBreakpoints.erase(mBreakpoints.begin() + i);
			break;
		}
	if (OnBreakpointRemove)
		OnBreakpointRemove(this, line);
}
void ImTextEdit::SetBreakpointEnabled(int line, bool enable)
{
	for (int i = 0; i < mBreakpoints.size(); i++)
		if (mBreakpoints[i].mLine == line) {
			mBreakpoints[i].mEnabled = enable;
			if (OnBreakpointUpdate)
				OnBreakpointUpdate(this, line, mBreakpoints[i].mUseCondition, mBreakpoints[i].mCondition, enable);
			break;
		}
}
ImTextEdit::Breakpoint& ImTextEdit::GetBreakpoint(int line)
{
	for (int i = 0; i < mBreakpoints.size(); i++)
		if (mBreakpoints[i].mLine == line)
			return mBreakpoints[i];
}

void ImTextEdit::RenderInternal(const char* aTitle)
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
	mFocused = ImGui::IsWindowFocused() || mFindFocused || mReplaceFocused;

	auto contentSize = ImGui::GetWindowContentRegionMax();
	auto drawList = ImGui::GetWindowDrawList();
	float longest(mTextStart);

	if (mScrollToTop)
	{
		mScrollToTop = false;
		ImGui::SetScrollY(0.f);
	}

	ImVec2 cursorScreenPos = mUICursorPos = ImGui::GetCursorScreenPos();
	auto scrollX = ImGui::GetScrollX();
	auto scrollY = mLastScroll = ImGui::GetScrollY();

	int pageSize = (int)floor((scrollY + contentSize.y) / mCharAdvance.y);
	auto lineNo = (int)floor(scrollY / mCharAdvance.y);
	auto globalLineMax = (int)mLines.size();
	auto lineMax = std::max<int>(0, std::min<int>((int)mLines.size() - 1, lineNo + pageSize));
	int totalLinesFolded = 0;

	// Deduce mTextStart by evaluating mLines size (global lineMax) plus two spaces as text width
	char buf[16];
	snprintf(buf, 16, " %3d ", globalLineMax);
	mTextStart = (ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf, nullptr, nullptr).x + mLeftMargin) * mSidebar;

	// render
	GetPageSize();
	if (!mLines.empty())
	{
		float spaceSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, " ", nullptr, nullptr).x;
		
		// find bracket pairs to highlight
		bool highlightBrackets = false;
		Coordinates highlightBracketCoord = mState.mCursorPosition, highlightBracketCursor = mState.mCursorPosition;
		if (mHighlightBrackets && mState.mSelectionStart == mState.mSelectionEnd) {
			if (mState.mCursorPosition.mLine <= mLines.size()) {
				int lineSize = mLines[mState.mCursorPosition.mLine].size();
				Coordinates start = GetCorrectCursorPosition();
				start.mColumn = std::min<int>(start.mColumn, mLines[start.mLine].size()-1);
				highlightBracketCursor = start;

				if (lineSize != 0 && start.mLine >= 0 && start.mLine < mLines.size() && start.mColumn >= 0 && start.mColumn < mLines[start.mLine].size()) {
					char c1 = mLines[start.mLine][start.mColumn].mChar;
					char c2 = mLines[start.mLine][std::max<int>(0, start.mColumn - 1)].mChar;
					int b1 = isBracket(c1);
					int b2 = isBracket(c2);

					if (b1 || b2) {
						char search = c1;
						int dir = b1;
						if (b2) {
							search = c2;
							dir = b2;
							start.mColumn = std::max<int>(0, start.mColumn - 1);
							highlightBracketCursor = start;
						}

						int weight = 0;

						// go forward
						if (dir == 1) {
							while (start.mLine < mLines.size()) {
								for (; start.mColumn < mLines[start.mLine].size(); start.mColumn++) {
									char curChar = mLines[start.mLine][start.mColumn].mChar;
									if (curChar == search)
										weight++;
									else if (isClosingBracket(search, curChar)) {
										weight--;
										if (weight <= 0) {
											highlightBrackets = true;
											highlightBracketCoord = start;
											break;
										}
									}
								}
								
								if (highlightBrackets)
									break;

								start.mLine++;
								start.mColumn = 0;
							}
						}
						// go backwards
						else {
							while (start.mLine >= 0) {
								for (; start.mColumn >= 0; start.mColumn--) {
									char curChar = mLines[start.mLine][start.mColumn].mChar;
									if (curChar == search)
										weight++;
									else if (isOpeningBracket(search, curChar)) {
										weight--;
										if (weight <= 0) {
											highlightBrackets = true;
											highlightBracketCoord = start;
											break;
										}
									}
								}

								if (highlightBrackets)
									break;

								start.mLine--;

								if (start.mLine >= 0)
									start.mColumn = mLines[start.mLine].size() - 1;
							}
						}
					}
				}
			}
		}

		// fold info
		int hoverFoldWeight = 0;
		int linesFolded = 0;
		uint64_t curTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		if (mFoldEnabled) {
			if (curTime - mFoldLastIteration > 3000) {
				// sort if needed
				if (!mFoldSorted) {
					std::sort(mFoldBegin.begin(), mFoldBegin.end());
					std::sort(mFoldEnd.begin(), mFoldEnd.end());
					mFoldSorted = true;
				}

				// resize if needed
				if (mFold.size() != mFoldBegin.size()) {
					mFold.resize(mFoldBegin.size(), false);
					mFoldConnection.resize(mFoldBegin.size(), -1);
				}

				// reconnect every fold BEGIN with END (TODO: any better way to do this?)
				std::vector<bool> foldUsed(mFoldEnd.size(), false);
				for (int i = mFoldBegin.size() - 1; i >= 0; i--) {
					int j = mFoldEnd.size() - 1;
					int lastUnused = j;
					for (; j >= 0; j--) {
						if (mFoldEnd[j] < mFoldBegin[i])
							break;
						if (!foldUsed[j])
							lastUnused = j;
					}

					if (lastUnused < mFoldEnd.size()) {
						foldUsed[lastUnused] = true;
						mFoldConnection[i] = lastUnused;
					}
				}

				mFoldLastIteration = curTime;
			}
		
			auto foldLineStart = 0;
			auto foldLineEnd = std::min<int>((int)mLines.size() - 1, lineNo);
			while (foldLineStart < mLines.size()) {
				// check if line is folded
				for (int i = 0; i < mFoldBegin.size(); i++) {
					if (mFoldBegin[i].mLine == foldLineStart) {
						if (i < mFold.size() && mFold[i]) {
							int foldCon = mFoldConnection[i];
							if (foldCon != -1 && foldCon < mFoldEnd.size()) {
								int diff = mFoldEnd[foldCon].mLine - mFoldBegin[i].mLine;
								if (foldLineStart < foldLineEnd) {
									linesFolded += diff;
									foldLineEnd = std::min<int>((int)mLines.size() - 1, foldLineEnd + diff);
								}

								totalLinesFolded += diff;
							}
							break;
						}
					}
				}
				foldLineStart++;
			}
			lineNo += linesFolded;
			lineMax = std::max<int>(0, std::min<int>((int)mLines.size() - 1, lineNo + pageSize));
		}

		// render
		while (lineNo <= lineMax)
		{
			ImVec2 lineStartScreenPos = ImVec2(cursorScreenPos.x, cursorScreenPos.y + (lineNo - linesFolded) * mCharAdvance.y);
			ImVec2 textScreenPos = ImVec2(lineStartScreenPos.x + mTextStart, lineStartScreenPos.y);

			auto* line = &mLines[lineNo];
			longest = std::max(mTextStart + TextDistanceToLineStart(Coordinates(lineNo, GetLineMaxColumn(lineNo))), longest);
			Coordinates lineStartCoord(lineNo, 0);
			Coordinates lineEndCoord(lineNo, GetLineMaxColumn(lineNo));

			// check if line is folded
			bool lineFolded = false;
			int lineNew = 0;
			Coordinates lineFoldStart, lineFoldEnd;
			int lineFoldStartCIndex = 0;
			if (mFoldEnabled) {
				for (int i = 0; i < mFoldBegin.size(); i++) {
					if (mFoldBegin[i].mLine == lineNo) {
						if (i < mFold.size()) {
							lineFolded = mFold[i];
							lineFoldStart = mFoldBegin[i];
							lineFoldStartCIndex = GetCharacterIndex(lineFoldStart);
							
							int foldCon = mFoldConnection[i];
							if (lineFolded && foldCon != -1 && foldCon < mFoldEnd.size())
								lineFoldEnd = mFoldEnd[foldCon];
						}
						break;
					}
				}
			}

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

			// draw snippet stuff
			if (mIsSnippet) {
				unsigned int oldColor = mPalette[(int)PaletteIndex::Selection];
				unsigned int alpha = (oldColor & 0xFF000000) >> 25;
				unsigned int newColor = (oldColor & 0x00FFFFFF) | (alpha << 24);

				for (int i = 0; i < mSnippetTagStart.size(); i++) {
					if (mSnippetTagStart[i].mLine == lineNo && mSnippetTagHighlight[i]) {
						float tstart = TextDistanceToLineStart(mSnippetTagStart[i]);
						float tend = TextDistanceToLineStart(mSnippetTagEnd[i]);

						ImVec2 vstart(lineStartScreenPos.x + mTextStart + tstart, lineStartScreenPos.y);
						ImVec2 vend(lineStartScreenPos.x + mTextStart + tend, lineStartScreenPos.y + mCharAdvance.y);
						drawList->AddRectFilled(vstart, vend, newColor);
					}
				}
			}

			auto start = ImVec2(lineStartScreenPos.x + scrollX, lineStartScreenPos.y);

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

			// Highlight the current line (where the cursor is)
			if (mState.mCursorPosition.mLine == lineNo)
			{
				auto focused = ImGui::IsWindowFocused();

				// Highlight the current line (where the cursor is)
				if (mHighlightLine && !HasSelection()) {
					auto end = ImVec2(start.x + contentSize.x + scrollX, start.y + mCharAdvance.y + 2.0f);
					drawList->AddRectFilled(start, end, mPalette[(int)(focused ? PaletteIndex::CurrentLineFill : PaletteIndex::CurrentLineFillInactive)]);
					drawList->AddRect(start, end, mPalette[(int)PaletteIndex::CurrentLineEdge], 1.0f);
				}

				// Render the cursor
				if (focused)
				{
					auto elapsed = curTime - mStartTime;
					if (elapsed > 400)
					{
						float width = 1.0f;
						auto cindex = GetCharacterIndex(mState.mCursorPosition);
						float cx = TextDistanceToLineStart(mState.mCursorPosition);

						if (mOverwrite && cindex < (int)line->size())
						{
							auto c = (*line)[cindex].mChar;
							if (c == '\t')
							{
								auto x = (1.0f + std::floor((1.0f + cx) / (float(mTabSize) * spaceSize))) * (float(mTabSize) * spaceSize);
								width = x - cx;
							}
							else
							{
								char buf2[2];
								buf2[0] = (*line)[cindex].mChar;
								buf2[1] = '\0';
								width = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf2).x;
							}
						}
						ImVec2 cstart(textScreenPos.x + cx, lineStartScreenPos.y);
						ImVec2 cend(textScreenPos.x + cx + width, lineStartScreenPos.y + mCharAdvance.y);
						drawList->AddRectFilled(cstart, cend, mPalette[(int)PaletteIndex::Cursor]);
						if (elapsed > 800)
							mStartTime = curTime;
					}
				}
			}

			// highlight the user defined lines
			if (mHighlightLine) {
				if (std::find(mHighlightedLines.begin(), mHighlightedLines.end(), lineNo) != mHighlightedLines.end()) {
					auto end = ImVec2(start.x + contentSize.x + scrollX, start.y + mCharAdvance.y);
					drawList->AddRectFilled(start, end, mPalette[(int)(PaletteIndex::CurrentLineFill)]);
				}
			}

			// text
			auto prevColor = line->empty() ? mPalette[(int)PaletteIndex::Default] : GetGlyphColor((*line)[0]);
			ImVec2 bufferOffset;
			for (int i = 0; i < line->size();)
			{
				auto& glyph = (*line)[i];
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

				// highlight brackets
				if (highlightBrackets) {
					if ((lineNo == highlightBracketCoord.mLine && i == highlightBracketCoord.mColumn) ||
						(lineNo == highlightBracketCursor.mLine && i == highlightBracketCursor.mColumn)) {
						const ImVec2 p1(textScreenPos.x + bufferOffset.x, textScreenPos.y + bufferOffset.y);
						const ImVec2 p2(textScreenPos.x + bufferOffset.x + ImGui::GetFont()->GetCharAdvance(mLines[highlightBracketCoord.mLine][highlightBracketCoord.mColumn].mChar), textScreenPos.y + bufferOffset.y + ImGui::GetFontSize());
						drawList->AddRectFilled(p1, p2, mPalette[(int)PaletteIndex::Selection]);
					}
				}

				// tab, space, etc...
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
						mLineBuffer.push_back((*line)[i++].mChar);
				}

				// skip if folded
				if (lineFolded && lineFoldStartCIndex == i - 1) {
					i = GetCharacterIndex(lineFoldEnd);
					lineNew = lineFoldEnd.mLine;
					line = &mLines[lineNew];
					lineFolded = false;
					if (!mLineBuffer.empty()) {
						// render the actual text
						const ImVec2 newOffset(textScreenPos.x + bufferOffset.x, textScreenPos.y + bufferOffset.y);
						auto textSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, mLineBuffer.c_str(), nullptr, nullptr);
						drawList->AddText(newOffset, prevColor, mLineBuffer.c_str());
						mLineBuffer.clear();
						bufferOffset.x += textSize.x;

						// render the [...] when folded
						const ImVec2 offsetFoldBox(textScreenPos.x + bufferOffset.x, textScreenPos.y + bufferOffset.y);
						drawList->AddText(offsetFoldBox, mPalette[(int)PaletteIndex::Default], " ... ");
						textSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, " ... ", nullptr, nullptr);
						drawList->AddRect(ImVec2(textScreenPos.x + bufferOffset.x + mCharAdvance.x / 2.0f, textScreenPos.y + bufferOffset.y),
										  ImVec2(textScreenPos.x + bufferOffset.x + textSize.x - mCharAdvance.x / 2.0f, textScreenPos.y + bufferOffset.y + mCharAdvance.y),
										  mPalette[(int)PaletteIndex::Default]);
						bufferOffset.x += textSize.x;
					}
				}

			}
			if (!mLineBuffer.empty())
			{
				const ImVec2 newOffset(textScreenPos.x + bufferOffset.x, textScreenPos.y + bufferOffset.y);
				drawList->AddText(newOffset, prevColor, mLineBuffer.c_str());
				mLineBuffer.clear();
			}

			// side bar
			if (mSidebar) {
				drawList->AddRectFilled(ImVec2(lineStartScreenPos.x + scrollX, lineStartScreenPos.y), ImVec2(lineStartScreenPos.x + scrollX + mTextStart - 5.0f, lineStartScreenPos.y + mCharAdvance.y), ImGui::GetColorU32(ImGuiCol_WindowBg));

				// Draw breakpoints
				if (HasBreakpoint(lineNo + 1) != 0) {
					float radius = ImGui::GetFontSize() * 1.0f / 3.0f;
					float startX = lineStartScreenPos.x + scrollX + radius + 2.0f;
					float startY = lineStartScreenPos.y + radius + 4.0f;

					drawList->AddCircle(ImVec2(startX, startY), radius + 1, mPalette[(int)PaletteIndex::BreakpointOutline]);
					drawList->AddCircleFilled(ImVec2(startX, startY), radius, mPalette[(int)PaletteIndex::Breakpoint]);

					Breakpoint bkpt = GetBreakpoint(lineNo + 1);
					if (!bkpt.mEnabled)
						drawList->AddCircleFilled(ImVec2(startX, startY), radius - 1, mPalette[(int)PaletteIndex::BreakpointDisabled]);
					else {
						if (bkpt.mUseCondition)
							drawList->AddRectFilled(ImVec2(startX - radius + 3, startY - radius / 4), ImVec2(startX + radius - 3, startY + radius / 4), mPalette[(int)PaletteIndex::BreakpointOutline]);
					}
				}

				// Draw current line indicator
				if (lineNo + 1 == mDebugCurrentLine) {
					float radius = ImGui::GetFontSize() * 1.0f / 3.0f;
					float startX = lineStartScreenPos.x + scrollX + radius + 2.0f;
					float startY = lineStartScreenPos.y + 4.0f;

					// outline
					drawList->AddRect(
						ImVec2(startX - radius, startY + radius / 2), ImVec2(startX, startY + radius * 3.0f / 2.0f),
						mPalette[(int)PaletteIndex::CurrentLineIndicatorOutline]);
					drawList->AddTriangle(
						ImVec2(startX - 1, startY - 2), ImVec2(startX - 1, startY + radius * 2 + 1), ImVec2(startX + radius, startY + radius),
						mPalette[(int)PaletteIndex::CurrentLineIndicatorOutline]);

					// fill
					drawList->AddRectFilled(
						ImVec2(startX - radius + 1, startY + 1 + radius / 2), ImVec2(startX + 1, startY - 1 + radius * 3.0f / 2.0f),
						mPalette[(int)PaletteIndex::CurrentLineIndicator]);
					drawList->AddTriangleFilled(
						ImVec2(startX, startY + 1), ImVec2(startX, startY - 1 + radius * 2), ImVec2(startX - 1 + radius, startY + radius),
						mPalette[(int)PaletteIndex::CurrentLineIndicator]);
				}

				// Draw line number (right aligned)
				if (mShowLineNumbers) {
					snprintf(buf, 16, "%3d  ", lineNo + 1);

					auto lineNoWidth = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf, nullptr, nullptr).x;
					drawList->AddText(ImVec2(lineStartScreenPos.x + scrollX + mTextStart - lineNoWidth, lineStartScreenPos.y), mPalette[(int)PaletteIndex::LineNumber], buf);
				}

				// fold +/- icon
				if (mFoldEnabled) {
					int foldID = 0;
					int foldWeight = 0;
					bool hasFold = false;
					bool hasFoldEnd = false;
					bool isFolded = false;
					float foldBtnSize = spaceSize;
					float foldStartX = lineStartScreenPos.x + scrollX + mTextStart - spaceSize * 2.0f + 4;
					float foldStartY = lineStartScreenPos.y + (ImGui::GetFontSize() - foldBtnSize) / 2.0f;

					// calculate current weight + find if here ends or starts another "fold"
					for (int i = 0; i < mFoldBegin.size(); i++) {
						if (mFoldBegin[i].mLine == lineNo) {
							hasFold = true;
							foldID = i;
							if (i < mFold.size())
								isFolded = mFold[i];
							break;
						} else if (mFoldBegin[i].mLine < lineNo)
							foldWeight++;
					}
					for (int i = 0; i < mFoldEnd.size(); i++) {
						if (mFoldEnd[i].mLine == lineNo) {
							hasFoldEnd = true;
							break;
						} else if (mFoldEnd[i].mLine < lineNo)
							foldWeight--;
					}
					bool isHovered = (hoverFoldWeight && foldWeight >= hoverFoldWeight);

					// line
					if (foldWeight > 0 && !hasFold) {
						ImVec2 p1(foldStartX + foldBtnSize / 2, lineStartScreenPos.y);
						ImVec2 p2(p1.x, p1.y + mCharAdvance.y + 1.0f);

						drawList->AddLine(p1, p2, mPalette[(int)PaletteIndex::Default]);
					}

					// render the +/- box
					if (hasFold) {
						ImVec2 fmin(foldStartX, foldStartY + 2);
						ImVec2 fmax(fmin.x + foldBtnSize, fmin.y + foldBtnSize);

						// line up
						if (foldWeight > 0) {
							ImVec2 p1(foldStartX + foldBtnSize / 2, lineStartScreenPos.y);
							ImVec2 p2(p1.x, fmin.y);
							drawList->AddLine(p1, p2, mPalette[(int)PaletteIndex::Default]);
						}

						// fold button
						foldWeight++;
						drawList->AddRect(fmin, fmax, mPalette[(int)PaletteIndex::Default]);

						// check if mouse is over this fold button
						ImVec2 mpos = ImGui::GetMousePos();
						if (mpos.x >= fmin.x && mpos.x <= fmax.x &&
							mpos.y >= fmin.y && mpos.y <= fmax.y)
						{
							isHovered = true;
							hoverFoldWeight = foldWeight;
							if (isFolded)
								hoverFoldWeight = 0;

							ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);

							if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
								if (foldID < mFold.size()) {
									isFolded = !isFolded;
									mFold[foldID] = isFolded;
								}
							}
						}

						// minus
						drawList->AddLine(ImVec2(fmin.x + 3, (fmin.y + fmax.y) / 2.0f), ImVec2(fmax.x - 4, (fmin.y + fmax.y) / 2.0f), mPalette[(int)PaletteIndex::Default]);

						// plus
						if (isFolded)
							drawList->AddLine(ImVec2((fmin.x + fmax.x) / 2.0f, fmin.y + 3), ImVec2((fmin.x + fmax.x) / 2.0f, fmax.y - 4), mPalette[(int)PaletteIndex::Default]);

						// line down
						if (!isFolded || foldWeight > 1) {
							float lineContinueY = (lineStartScreenPos.y + mCharAdvance.y) - fmax.y;
							ImVec2 p1(foldStartX + foldBtnSize / 2, fmax.y);
							ImVec2 p2(p1.x, fmax.y + lineContinueY);
							drawList->AddLine(p1, p2, mPalette[(int)PaletteIndex::Default]);
						}
					}
					// horizontal line
					else if (hasFoldEnd) {
						foldWeight--;
						if (hoverFoldWeight && foldWeight < hoverFoldWeight)
							hoverFoldWeight = 0;

						ImVec2 p1(foldStartX + foldBtnSize / 2, lineStartScreenPos.y + mCharAdvance.y - 1.0f);
						ImVec2 p2(p1.x + mCharAdvance.x / 2.0f, p1.y);
						drawList->AddLine(p1, p2, mPalette[(int)PaletteIndex::Default]);
					}

					// hover background
					if (isHovered) {
						// sidebar bg
						ImVec2 pmin(foldStartX-4, lineStartScreenPos.y);
						ImVec2 pmax(pmin.x + foldBtnSize + 8, pmin.y + mCharAdvance.y);
						drawList->AddRectFilled(pmin, pmax, 0x40000000 | (0x00FFFFFF & mPalette[(int)PaletteIndex::Default]));
						
						// line bg
						pmin.x = pmax.x;
						pmax.x = lineStartScreenPos.x + scrollX + mTextStart + contentSize.x;
						drawList->AddRectFilled(pmin, pmax, 0x20000000 | (0x00FFFFFF & mPalette[(int)PaletteIndex::Default]));
					}
				}
			}

			if (lineNew) {
				linesFolded += lineNew - lineNo;
				lineMax = std::min<int>((int)mLines.size() - 1, lineMax + lineNew - lineNo);
				lineNo = lineNew;
			}

			++lineNo;
		}

		// Draw a tooltip on known identifiers/preprocessor symbols
		if (ImGui::IsMousePosValid() && (IsDebugging() || mFuncTooltips || ImGui::GetIO().KeyCtrl))
		{
			Coordinates hoverPosition = MousePosToCoordinates(ImGui::GetMousePos());
			if (hoverPosition != mLastHoverPosition) {
				mLastHoverPosition = hoverPosition;
				mLastHoverTime = std::chrono::steady_clock::now();
			}
			
			Char hoverChar = 0;
			if (hoverPosition.mLine < mLines.size() && hoverPosition.mColumn < mLines[hoverPosition.mLine].size())
				hoverChar = mLines[hoverPosition.mLine][hoverPosition.mColumn].mChar;

			double hoverTime = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - mLastHoverTime).count();
			
			if (hoverTime > 0.5 && (hoverChar == '(' || hoverChar == ')') && IsDebugging()) {
				std::string expr = "";

				int colStart = 0, rowStart = hoverPosition.mLine;
				int bracketMatch = 0;
				if (hoverChar == ')') {
					int colIndex = hoverPosition.mColumn;
					for (; rowStart >= 0; rowStart--) {
						for (int i = colIndex; i >= 0; i--) {
							char curChar = mLines[rowStart][i].mChar;
							if (curChar == '(')
								bracketMatch++;
							else if (curChar == ')')
								bracketMatch--;

							if (!isspace(curChar) || curChar == ' ')
								expr += curChar;

							if (bracketMatch == 0) {
								colStart = i - 1;
								break;
							}
						}
						if (bracketMatch == 0)
							break;
						if (rowStart != 0)
							colIndex = mLines[rowStart - 1].size() - 1;
					}
					std::reverse(expr.begin(), expr.end());

					if (rowStart <= 0)
						colStart = -1;
				} else if (hoverChar == '(') {
					int colIndex = hoverPosition.mColumn;
					colStart = hoverPosition.mColumn - 1;
					for (int j = rowStart; j < mLines.size(); j++) {
						for (int i = colIndex; i < mLines[j].size(); i++) {
							char curChar = mLines[j][i].mChar;
							if (curChar == '(')
								bracketMatch++;
							else if (curChar == ')')
								bracketMatch--;

							if (!isspace(curChar) || curChar == ' ')
								expr += curChar;

							if (bracketMatch == 0)
								break;
						}

						if (bracketMatch == 0)
							break;
						if (j != 0)
							colIndex = 0;
					}

					if (rowStart >= mLines.size())
						colStart = -1;
				}

				while (colStart >= 0 && isalnum(mLines[rowStart][colStart].mChar)) {
					expr.insert(expr.begin(), mLines[rowStart][colStart].mChar);
					colStart--;
				}

				if (OnExpressionHover && HasExpressionHover)
				{
					if (HasExpressionHover(this, expr)) {
						ImGui::BeginTooltip();
						OnExpressionHover(this, expr);
						ImGui::EndTooltip();
					}				}
			}
			else if (hoverTime > 0.2) {
				Coordinates wordCoords = ScreenPosToCoordinates(ImGui::GetMousePos());
				auto id = GetWordAt(wordCoords);
				bool isCtrlDown = ImGui::GetIO().KeyCtrl;

				if (!id.empty()) {
					// function/value tooltips
					if (!isCtrlDown) {
						auto it = mLanguageDefinition.mIdentifiers.find(id);
						if (it != mLanguageDefinition.mIdentifiers.end() && mFuncTooltips) {
							ImGui::BeginTooltip();
							ImGui::TextUnformatted(it->second.mDeclaration.c_str());
							ImGui::EndTooltip();
						} else {
							auto pi = mLanguageDefinition.mPreprocIdentifiers.find(id);
							if (pi != mLanguageDefinition.mPreprocIdentifiers.end() && mFuncTooltips) {
								ImGui::BeginTooltip();
								ImGui::TextUnformatted(pi->second.mDeclaration.c_str());
								ImGui::EndTooltip();
							} else if (IsDebugging() && OnIdentifierHover && HasIdentifierHover) {
								if (HasIdentifierHover(this, id)) {
									ImGui::BeginTooltip();
									OnIdentifierHover(this, id);
									ImGui::EndTooltip();
								}
							} else if (mACFunctions.count(id) && mFuncTooltips) {
								ImGui::BeginTooltip();
								ImGui::TextUnformatted(mBuildFunctionDef(id, mLanguageDefinition.mName).c_str());
								ImGui::EndTooltip();
							}
						}
					}
					// CTRL + click functionality
					else {
						bool hasUnderline = false;
						bool isHeader = false;
						bool isFindFirst = false;
						Coordinates findStart(0, 0);
						std::string header = "";
						Coordinates wordStart = FindWordStart(wordCoords);
						Coordinates wordEnd = FindWordEnd(wordCoords);

						// check if header
						if (wordStart.mLine >= 0 && wordStart.mLine < mLines.size()) {
							// get the contents of the line in std::string
							const auto& line = mLines[wordStart.mLine];
							std::string text;
							text.resize(line.size());
							for (size_t i = 0; i < line.size(); ++i)
								text[i] = line[i].mChar;

							// find #include
							size_t includeLoc = text.find("#include");
							size_t includeStart = std::string::npos, includeEnd = std::string::npos;
							if (text.size() > 0 && includeLoc != std::string::npos) {

								for (size_t f = includeLoc; f < text.size(); f++) {
									if (text[f] == '<' || text[f] == '\"')
										includeStart = f + 1;
									else if (text[f] == '>' || text[f] == '\"') {
										includeEnd = f - 1;
										break;
									}
								}
							}

							if (includeStart != std::string::npos && includeEnd != std::string::npos) {
								hasUnderline = true;
								isHeader = true;
								
								header = text.substr(includeStart, includeEnd-includeStart + 1);
								wordStart.mColumn = 0;
								wordEnd.mColumn = GetCharacterColumn(wordEnd.mLine, line.size());
							}
						}

						// check if mul, sin, cos, etc...
						if (!hasUnderline && mLanguageDefinition.mIdentifiers.find(id) != mLanguageDefinition.mIdentifiers.end()) 
							hasUnderline = true;

						// check if function
						if (!hasUnderline && mACFunctions.count(id) > 0)
							hasUnderline = true;

						// check if user type variable
						if (!hasUnderline && mACUserTypes.count(id) > 0) {
							hasUnderline = true;
							isFindFirst = true;
						}

						// check if local variable
						if (!hasUnderline) {
							for (const auto& func : mACFunctions) {
								if (wordCoords.mLine + 1 >= func.second.LineStart && wordCoords.mLine + 1 <= func.second.LineEnd) {
									for (const auto& local : func.second.Locals) {
										if (local.Name == id) {
											hasUnderline = true;
											isFindFirst = true;
											findStart = Coordinates(func.second.LineStart - 1, 0);
											break;
										}
									}
									break;
								}
							}
						}

						// check if global/uniform variable
						if (!hasUnderline) {
							for (const auto& glob : mACGlobals)
								if (glob.Name == id) {
									hasUnderline = true;
									isFindFirst = true;
									break;
								}
							for (const auto& unif : mACUniforms) 
								if (unif.Name == id) {
									hasUnderline = true;
									isFindFirst = true;
									break;
								}
						}

						// draw the underline
						if (hasUnderline) {
							ImVec2 ulinePos = ImVec2(cursorScreenPos.x + mTextStart, cursorScreenPos.y + wordStart.mLine * mCharAdvance.y);
							drawList->AddLine(ImVec2(ulinePos.x + wordStart.mColumn * mCharAdvance.x, ulinePos.y + mCharAdvance.y),
								ImVec2(ulinePos.x + wordEnd.mColumn * mCharAdvance.x, ulinePos.y + mCharAdvance.y),
								ImGui::GetColorU32(ImGuiCol_HeaderHovered));
							ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);

							if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
								if (isHeader) {
									if (RequestOpen)
										RequestOpen(this, this->mPath, header);
								}
								else {
									// go to function definition
									if (mACFunctions.count(id)) {
										int funcLine = mACFunctions[id].LineStart - 1;
										if (funcLine > 0 && funcLine < mLines.size()) {
											mState.mCursorPosition.mLine = funcLine;
											mState.mCursorPosition.mColumn = 0;
											EnsureCursorVisible();
										}
									}

									// go to type definition
									if (isFindFirst) {
										if (!ImGui::GetIO().KeyAlt) {
											Coordinates userTypeLoc = FindFirst(id, findStart);
											if (userTypeLoc.mLine >= 0 && userTypeLoc.mLine < mLines.size()) {
												mState.mCursorPosition = userTypeLoc;
												SetSelectionStart(userTypeLoc);
												SetSelectionEnd(FindWordEnd(userTypeLoc));
												EnsureCursorVisible();
											}
										}
										else {
											if (OnCtrlAltClick) 
												this->OnCtrlAltClick(this, id, wordCoords);
										}
									}

									// TODO: identifier documentation
								}
							}
						}
					}
				}
			}
		}
	}

	// function tooltip
	if (mFunctionDeclarationTooltip) {
		const float ttWidth = 350, ttHeight = 50;
		ImVec2 ttPos = CoordinatesToScreenPos(mFunctionDeclarationCoord);
		ttPos.y += mCharAdvance.y;
		ttPos.x += ImGui::GetScrollX();

		drawList->AddRectFilled(ttPos, ImVec2(ttPos.x + mUICalculateSize(ttWidth), ttPos.y + mUICalculateSize(ttHeight)), ImGui::GetColorU32(ImGuiCol_FrameBg));

		ImFont* font = ImGui::GetFont();
		ImGui::PopFont();

		ImGui::SetNextWindowPos(ttPos, ImGuiCond_Always);
		ImGui::BeginChild("##texteditor_functooltip", ImVec2(mUICalculateSize(ttWidth), mUICalculateSize(ttHeight)), true);

		ImGui::TextWrapped("%s", mFunctionDeclaration.c_str());

		ImGui::EndChild();

		ImGui::PushFont(font);

		ImGui::SetWindowFocus();
		if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape)))
			mFunctionDeclarationTooltip = false;
	}

	// suggestions window
	if (mACOpened) {
		auto acCoord = FindWordStart(mACPosition);
		ImVec2 acPos = CoordinatesToScreenPos(acCoord);
		acPos.y += mCharAdvance.y;
		acPos.x += ImGui::GetScrollX();

		drawList->AddRectFilled(acPos, ImVec2(acPos.x + mUICalculateSize(150), acPos.y + mUICalculateSize(100)), ImGui::GetColorU32(ImGuiCol_FrameBg));
	
		ImFont* font = ImGui::GetFont();
		ImGui::PopFont();

		ImGui::SetNextWindowPos(acPos, ImGuiCond_Always);
		ImGui::BeginChild("##texteditor_autocompl", ImVec2(mUICalculateSize(150), mUICalculateSize(100)), true);
		
		for (int i = 0; i < mACSuggestions.size(); i++) {
			ImGui::Selectable(mACSuggestions[i].first.c_str(), i == mACIndex);
			if (i == mACIndex)
				ImGui::SetScrollHereY();
		}

		ImGui::EndChild();

		ImGui::PushFont(font);

		ImGui::SetWindowFocus();
		if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape))) {
			mACOpened = false;
			mACObject = "";
		}
	}

	ImGui::Dummy(ImVec2(longest + mEditorCalculateSize(100), (mLines.size() - totalLinesFolded) * mCharAdvance.y));

	if (mDebugCurrentLineUpdated) {
		float scrollX = ImGui::GetScrollX();
		float scrollY = ImGui::GetScrollY();

		auto height = ImGui::GetWindowHeight();
		auto width = ImGui::GetWindowWidth();

		auto top = 1 + (int)ceil(scrollY / mCharAdvance.y);
		auto bottom = (int)ceil((scrollY + height) / mCharAdvance.y);

		auto left = (int)ceil(scrollX / mCharAdvance.x);
		auto right = (int)ceil((scrollX + width) / mCharAdvance.x);

		ImTextEdit::Coordinates pos(mDebugCurrentLine, 0);

		if (pos.mLine < top)
			ImGui::SetScrollY(std::max(0.0f, (pos.mLine - 1) * mCharAdvance.y));
		if (pos.mLine > bottom - 4)
			ImGui::SetScrollY(std::max(0.0f, (pos.mLine + 4) * mCharAdvance.y - height));

		mDebugCurrentLineUpdated = false;
	}

	if (mScrollToCursor)
	{
		EnsureCursorVisible();
		ImGui::SetWindowFocus();
		mScrollToCursor = false;
	}

	
	// hacky way to get the bg working
	if (mFindOpened)
	{
		ImVec2 findPos = ImVec2(mUICursorPos.x + scrollX + mWindowWidth - mUICalculateSize(250), mUICursorPos.y + ImGui::GetScrollY() + mUICalculateSize(50) * (IsDebugging() && mDebugBar));
		drawList->AddRectFilled(findPos, ImVec2(findPos.x + mUICalculateSize(220), findPos.y + mUICalculateSize(mReplaceOpened ? 90 : 40)), ImGui::GetColorU32(ImGuiCol_WindowBg));
	}
	if (IsDebugging() && mDebugBar)
	{
		ImVec2 dbgPos = ImVec2(mUICursorPos.x + scrollX + mWindowWidth / 2 - mDebugBarWidth / 2, mUICursorPos.y + ImGui::GetScrollY());
		drawList->AddRectFilled(dbgPos, ImVec2(dbgPos.x + mDebugBarWidth, dbgPos.y + mDebugBarHeight), ImGui::GetColorU32(ImGuiCol_FrameBg));
	}
}

void ImTextEdit::mOpenFunctionDeclarationTooltip(const std::string& obj, ImTextEdit::Coordinates coord)
{
	if (mACFunctions.count(obj)) {
		mFunctionDeclarationTooltip = true;
		mFunctionDeclarationCoord = FindWordStart(coord);
		mFunctionDeclaration = mBuildFunctionDef(obj, mLanguageDefinition.mName);
	}
}

std::string ImTextEdit::mBuildFunctionDef(const std::string& func, const std::string& lang)
{
	if (mACFunctions.count(func) == 0)
		return "";

	const auto& funcDef = mACFunctions[func];

	std::string ret = mBuildVariableType(funcDef.ReturnType, lang) + " " + func + "(";

	for (size_t i = 0; i < funcDef.Arguments.size(); i++) {
		ret += mBuildVariableType(funcDef.Arguments[i], lang) + " " + funcDef.Arguments[i].Name;

		if (i != funcDef.Arguments.size() - 1)
			ret += ", ";
	}

	return ret + ")";
}
std::string ImTextEdit::mBuildVariableType(const ed::SPIRVParser::Variable& var, const std::string& lang)
{
	switch (var.Type) {
		case ed::SPIRVParser::ValueType::Bool:
			return "bool";

		case ed::SPIRVParser::ValueType::Float:
			return "float";

		case ed::SPIRVParser::ValueType::Int:
			return "int";

		case ed::SPIRVParser::ValueType::Void:
			return "void";

		case ed::SPIRVParser::ValueType::Struct:
			return var.TypeName;

		case ed::SPIRVParser::ValueType::Vector: {
			std::string count = std::string(1, var.TypeComponentCount + '0');
			if (lang == "HLSL") {
				switch (var.BaseType) {
				case ed::SPIRVParser::ValueType::Bool:
					return "bool" + count;

				case ed::SPIRVParser::ValueType::Float:
					return "float" + count;

				case ed::SPIRVParser::ValueType::Int:
					return "int" + count;
				}
			} else {
				switch (var.BaseType) {
				case ed::SPIRVParser::ValueType::Bool:
					return "bvec" + count;

				case ed::SPIRVParser::ValueType::Float:
					return "vec" + count;

				case ed::SPIRVParser::ValueType::Int:
					return "ivec" + count;
				}
			}
		} break;

		case ed::SPIRVParser::ValueType::Matrix: {
			std::string count = std::string(1, var.TypeComponentCount + '0');
			if (lang == "HLSL") {
				return "float" + count + "x" + count;
			} else {
				return "mat" + count;
			}
		} break;
	}

	return "";
}

void ImTextEdit::mRemoveFolds(const Coordinates& aStart, const Coordinates& aEnd)
{
	mRemoveFolds(mFoldBegin, aStart, aEnd);
	mRemoveFolds(mFoldEnd, aStart, aEnd);
}
void ImTextEdit::mRemoveFolds(std::vector<Coordinates>& folds, const Coordinates& aStart, const Coordinates& aEnd)
{
	bool deleteFullyLastLine = false;
	if (aEnd.mLine >= mLines.size() || aEnd.mColumn >= 100000)
		deleteFullyLastLine = true;

	for (int i = 0; i < folds.size(); i++) {
		if (folds[i].mLine >= aStart.mLine && folds[i].mLine <= aEnd.mLine) {
			if (folds[i].mLine == aStart.mLine && aStart.mLine != aEnd.mLine) {
				if (folds[i].mColumn >= aStart.mColumn) {
					folds.erase(folds.begin() + i);
					mFoldSorted = false;
					i--;
				}
			} else if (folds[i].mLine == aEnd.mLine) {
				if (folds[i].mColumn < aEnd.mColumn) {
					if (aEnd.mLine != aStart.mLine || folds[i].mColumn >= aStart.mColumn) {
						folds.erase(folds.begin() + i);
						mFoldSorted = false;
						i--;
					}
				} else {
					if (aEnd.mLine == aStart.mLine)
						folds[i].mColumn = std::max<int>(0, folds[i].mColumn - (aEnd.mColumn - aStart.mColumn));
					else {
						// calculate new
						if (aStart.mLine < mLines.size()) {
							auto* line = &mLines[aStart.mLine];
							int colOffset = 0;
							int chi = 0;
							bool skipped = false;
							int bracketEndChIndex = GetCharacterIndex(mFoldEnd[i]);
							while (chi < (int)line->size() && (!skipped || (skipped && chi < bracketEndChIndex))) {
								auto c = (*line)[chi].mChar;
								chi += UTF8CharLength(c);
								if (c == '\t')
									colOffset = (colOffset / mTabSize) * mTabSize + mTabSize;
								else
									colOffset++;

								// go to the last line
								if (chi == line->size() && aEnd.mLine < mLines.size() && !skipped) {
									chi = GetCharacterIndex(aEnd);
									line = &mLines[aEnd.mLine];
									skipped = true;
								}
							}
							folds[i].mColumn = colOffset;
						}

						folds[i].mLine -= (aEnd.mLine - aStart.mLine);
					}
				}
			} else {
				folds.erase(folds.begin() + i);
				mFoldSorted = false;
				i--;
			}
		} else if (folds[i].mLine > aEnd.mLine)
			folds[i].mLine -= (aEnd.mLine - aStart.mLine) + deleteFullyLastLine;
	}
}

std::string ImTextEdit::mAutcompleteParse(const std::string& str, const Coordinates& start)
{
	const char* buffer = str.c_str();
	const char* tagPlaceholderStart = buffer;
	const char* tagStart = buffer;

	bool parsingTag = false;
	bool parsingTagPlaceholder = false;

	std::vector<int> tagIds, tagLocations, tagLengths;
	std::unordered_map<int, std::string> tagPlaceholders;

	mSnippetTagStart.clear();
	mSnippetTagEnd.clear();
	mSnippetTagID.clear();
	mSnippetTagHighlight.clear();

	Coordinates cursor = start, tagStartCoord, tagEndCoord;

	int tagId = -1;

	int modif = 0;
	while (*buffer != '\0') {
		if (*buffer == '{' && *(buffer + 1) == '$') {
			parsingTagPlaceholder = false;
			parsingTag = true;
			tagId = -1;
			tagStart = buffer;

			tagStartCoord = cursor;

			const char* skipBuffer = buffer;
			char** endLoc = const_cast<char**>(&buffer); // oops
			tagId = strtol(buffer + 2, endLoc, 10);

			cursor.mColumn += *endLoc - skipBuffer;

			if (*buffer == ':') {
				tagPlaceholderStart = buffer + 1;
				parsingTagPlaceholder = true;
			}
		}
		
		if (*buffer == '}' && parsingTag) {
			std::string tagPlaceholder = "";
			if (parsingTagPlaceholder)
				tagPlaceholder = std::string(tagPlaceholderStart, buffer - tagPlaceholderStart);

			tagIds.push_back(tagId);
			tagLocations.push_back(tagStart - str.c_str());
			tagLengths.push_back(buffer - tagStart + 1);
			if (!tagPlaceholder.empty() || tagPlaceholders.count(tagId) == 0) {
				if (tagPlaceholder.empty())
					tagPlaceholder = " ";
				
				tagStartCoord.mColumn = std::max<int>(0, tagStartCoord.mColumn - modif);
				tagEndCoord = tagStartCoord;
				tagEndCoord.mColumn += tagPlaceholder.size();

				mSnippetTagStart.push_back(tagStartCoord);
				mSnippetTagEnd.push_back(tagEndCoord);
				mSnippetTagID.push_back(tagId);
				mSnippetTagHighlight.push_back(true);

				tagPlaceholders[tagId] = tagPlaceholder;
			} else {
				tagStartCoord.mColumn = std::max<int>(0, tagStartCoord.mColumn - modif);
				tagEndCoord = tagStartCoord;
				tagEndCoord.mColumn += tagPlaceholders[tagId].size();

				mSnippetTagStart.push_back(tagStartCoord);
				mSnippetTagEnd.push_back(tagEndCoord);
				mSnippetTagID.push_back(tagId);
				mSnippetTagHighlight.push_back(false);
			}
			modif += (tagLengths.back() - tagPlaceholders[tagId].size());


			parsingTagPlaceholder = false;
			parsingTag = false;
			tagId = -1;
		}

		if (*buffer == '\n') {
			cursor.mLine++;
			cursor.mColumn = 0;
			modif = 0;
		} else 
			cursor.mColumn++;

		buffer++;
	}

	mIsSnippet = !tagIds.empty();

	std::string ret = str;
	for (int i = tagLocations.size() - 1; i >= 0; i--) {
		ret.erase(tagLocations[i], tagLengths[i]);
		ret.insert(tagLocations[i], tagPlaceholders[tagIds[i]]);
	}

	return ret;
}
void ImTextEdit::mAutocompleteSelect()
{
	UndoRecord undo;
	undo.mBefore = mState;

	auto curCoord = GetCursorPosition();
	curCoord.mColumn = std::max<int>(curCoord.mColumn - 1, 0);

	auto acStart = FindWordStart(curCoord);
	auto acEnd = FindWordEnd(curCoord);

	if (!mACObject.empty())
		acStart = mACPosition;

	undo.mAddedStart = acStart;
	int undoPopCount = std::max(0, acEnd.mColumn - acStart.mColumn) + 1;

	if (!mACObject.empty() && mACWord.empty())
		undoPopCount = 0;

	const auto& acEntry = mACSuggestions[mACIndex];

	std::string entryText = mAutcompleteParse(acEntry.second, acStart);

	if (acStart.mColumn != acEnd.mColumn) {
		SetSelection(acStart, acEnd);
		Backspace();
	}
	InsertText(entryText, true);

	undo.mAdded = entryText;
	undo.mAddedEnd = GetActualCursorCoordinates();

	if (mIsSnippet && mSnippetTagStart.size() > 0) {
		SetSelection(mSnippetTagStart[0], mSnippetTagEnd[0]);
		SetCursorPosition(mSnippetTagEnd[0]);
		mSnippetTagSelected = 0;
		mSnippetTagLength = 0;
		mSnippetTagPreviousLength = mSnippetTagEnd[mSnippetTagSelected].mColumn - mSnippetTagStart[mSnippetTagSelected].mColumn;
	}

	m_requestAutocomplete = false;
	mACOpened = false;
	mACObject = "";
	
	undo.mAfter = mState;

	while (undoPopCount-- != 0) {
		mUndoIndex--;
		mUndoBuffer.pop_back();
	}
	AddUndo(undo);
}

void ImTextEdit::m_buildMemberSuggestions(bool* keepACOpened)
{
	mACSuggestions.clear();

	auto curPos = GetCorrectCursorPosition();
	std::string obj = GetWordAt(curPos);

	ed::SPIRVParser::Variable* var = nullptr;

	for (auto& func : mACFunctions) {
		// suggest arguments and locals
		if (mState.mCursorPosition.mLine >= func.second.LineStart - 2 && mState.mCursorPosition.mLine <= func.second.LineEnd + 1) {
			// locals
			for (auto& loc : func.second.Locals)
				if (strcmp(loc.Name.c_str(), obj.c_str()) == 0) {
					var = &loc;
					break;
				}

			// arguments
			if (var == nullptr) {
				for (auto& arg : func.second.Arguments)
					if (strcmp(arg.Name.c_str(), obj.c_str()) == 0) {
						var = &arg;
						break;
					}
			}
		}
	}
	if (var == nullptr) {
		for (auto& uni : mACUniforms)
			if (strcmp(uni.Name.c_str(), obj.c_str()) == 0) {
				var = &uni;
				break;
			}
	}
	if (var == nullptr) {
		for (auto& glob : mACGlobals)
			if (strcmp(glob.Name.c_str(), obj.c_str()) == 0) {
				var = &glob;
				break;
			}
	}

	if (var != nullptr) {
		mACIndex = 0;
		mACSwitched = false;

		if (var->TypeName.size() > 0 && var->TypeName[0] != 0) {
			for (const auto& uType : mACUserTypes) {
				if (uType.first == var->TypeName) {
					for (const auto& uMember : uType.second)
						mACSuggestions.push_back(std::make_pair(uMember.Name, uMember.Name));
				}
			}

			if (mACSuggestions.size() > 0)
				mACObject = var->TypeName;
		}
	}

	if (mACSuggestions.size() > 0) {
		mACOpened = true;
		mACWord = "";

		if (keepACOpened != nullptr)
			*keepACOpened = true;

		Coordinates curCursor = GetCursorPosition();

		mACPosition = FindWordStart(curCursor);
	}
}
void ImTextEdit::m_buildSuggestions(bool* keepACOpened)
{
	mACWord = GetWordUnderCursor();

	bool isValid = false;
	for (int i = 0; i < mACWord.size(); i++)
		if ((mACWord[i] >= 'a' && mACWord[i] <= 'z') || (mACWord[i] >= 'A' && mACWord[i] <= 'Z')) {
			isValid = true;
			break;
		}

	if (isValid) {
		mACSuggestions.clear();
		mACIndex = 0;
		mACSwitched = false;

		std::string acWord = mACWord;
		std::transform(acWord.begin(), acWord.end(), acWord.begin(), tolower);

		struct ACEntry {
			ACEntry(const std::string& str, const std::string& val, int loc)
			{
				DisplayString = str;
				Value = val;
				Location = loc;
			}

			std::string DisplayString;
			std::string Value;
			int Location;
		};
		std::vector<ACEntry> weights;

		if (mACObject.empty()) {
			// get the words
			for (int i = 0; i < mACEntrySearch.size(); i++) {
				std::string lwrStr = mACEntrySearch[i];
				std::transform(lwrStr.begin(), lwrStr.end(), lwrStr.begin(), tolower);

				size_t loc = lwrStr.find(acWord);
				if (loc != std::string::npos)
					weights.push_back(ACEntry(mACEntries[i].first, mACEntries[i].second, loc));
			}
			for (auto& func : mACFunctions) {
				std::string lwrStr = func.first;
				std::transform(lwrStr.begin(), lwrStr.end(), lwrStr.begin(), tolower);

				// suggest arguments and locals
				if (mState.mCursorPosition.mLine >= func.second.LineStart - 2 && mState.mCursorPosition.mLine <= func.second.LineEnd + 1) {

					// locals
					for (auto& loc : func.second.Locals) {
						std::string lwrLoc = loc.Name;
						std::transform(lwrLoc.begin(), lwrLoc.end(), lwrLoc.begin(), tolower);

						size_t location = lwrLoc.find(acWord);
						if (location != std::string::npos)
							weights.push_back(ACEntry(loc.Name, loc.Name, location));
					}

					// arguments
					for (auto& arg : func.second.Arguments) {
						std::string lwrLoc = arg.Name;
						std::transform(lwrLoc.begin(), lwrLoc.end(), lwrLoc.begin(), tolower);

						size_t loc = lwrLoc.find(acWord);
						if (loc != std::string::npos)
							weights.push_back(ACEntry(arg.Name, arg.Name, loc));
					}
				}

				size_t loc = lwrStr.find(acWord);
				if (loc != std::string::npos) {
					std::string val = func.first;
					if (mCompleteBraces) val += "()";
					weights.push_back(ACEntry(func.first, val, loc));
				}
			}
			for (auto& uni : mACUniforms) {
				std::string lwrStr = uni.Name;
				std::transform(lwrStr.begin(), lwrStr.end(), lwrStr.begin(), tolower);

				size_t loc = lwrStr.find(acWord);
				if (loc != std::string::npos)
					weights.push_back(ACEntry(uni.Name, uni.Name, loc));
			}
			for (auto& glob : mACGlobals) {
				std::string lwrStr = glob.Name;
				std::transform(lwrStr.begin(), lwrStr.end(), lwrStr.begin(), tolower);

				size_t loc = lwrStr.find(acWord);
				if (loc != std::string::npos)
					weights.push_back(ACEntry(glob.Name, glob.Name, loc));
			}
			for (auto& utype : mACUserTypes) {
				std::string lwrStr = utype.first;
				std::transform(lwrStr.begin(), lwrStr.end(), lwrStr.begin(), tolower);

				size_t loc = lwrStr.find(acWord);
				if (loc != std::string::npos)
					weights.push_back(ACEntry(utype.first, utype.first, loc));
			}
			for (auto& str : mLanguageDefinition.mKeywords) {
				std::string lwrStr = str;
				std::transform(lwrStr.begin(), lwrStr.end(), lwrStr.begin(), tolower);

				size_t loc = lwrStr.find(acWord);
				if (loc != std::string::npos)
					weights.push_back(ACEntry(str, str, loc));
			}
			for (auto& str : mLanguageDefinition.mIdentifiers) {
				std::string lwrStr = str.first;
				std::transform(lwrStr.begin(), lwrStr.end(), lwrStr.begin(), tolower);

				size_t loc = lwrStr.find(acWord);
				if (loc != std::string::npos) {
					std::string val = str.first;
					if (mCompleteBraces) val += "()";
					weights.push_back(ACEntry(str.first, val, loc));
				}
			}
		}
		else {
			for (const auto& uType : mACUserTypes) {
				if (uType.first == mACObject) {
					for (const auto& uMember : uType.second) {

						std::string lwrStr = uMember.Name;
						std::transform(lwrStr.begin(), lwrStr.end(), lwrStr.begin(), tolower);

						size_t loc = lwrStr.find(acWord);
						if (loc != std::string::npos)
							weights.push_back(ACEntry(uMember.Name, uMember.Name, loc));
					}
				}
			}

		}

		// build the actual list
		for (const auto& entry : weights)
			if (entry.Location == 0)
				mACSuggestions.push_back(std::make_pair(entry.DisplayString, entry.Value));
		for (const auto& entry : weights)
			if (entry.Location != 0)
				mACSuggestions.push_back(std::make_pair(entry.DisplayString, entry.Value));

		
		if (mACSuggestions.size() > 0) {
			mACOpened = true;

			if (keepACOpened != nullptr)
				*keepACOpened = true;

			Coordinates curCursor = GetCursorPosition();
			curCursor.mColumn--;

			mACPosition = FindWordStart(curCursor);
		}
	}

	
	
}

ImVec2 ImTextEdit::CoordinatesToScreenPos(const ImTextEdit::Coordinates& aPosition) const
{
	ImVec2 origin = mUICursorPos;
	int dist = aPosition.mColumn;

	int retY = origin.y + aPosition.mLine * mCharAdvance.y;
	int retX = origin.x + GetTextStart() * mCharAdvance.x + dist * mCharAdvance.x - ImGui::GetScrollX();

	return ImVec2(retX, retY);
}

void ImTextEdit::Render(const char* aTitle, const ImVec2& aSize, bool aBorder)
{
	mWithinRender = true;
	mCursorPositionChanged = false;

	mFindOrigin = ImGui::GetCursorScreenPos();
	float windowWidth = mWindowWidth = ImGui::GetWindowWidth();
	
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertU32ToFloat4(mPalette[(int)PaletteIndex::Background]));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
	if (!mIgnoreImGuiChild)
		ImGui::BeginChild(aTitle, aSize, aBorder, (ImGuiWindowFlags_AlwaysHorizontalScrollbar * mHorizontalScroll) | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoNav);
	
	if (mHandleKeyboardInputs)
	{
		HandleKeyboardInputs();
		ImGui::PushAllowKeyboardFocus(true);
	}

	if (mHandleMouseInputs)
		HandleMouseInputs();

	ColorizeInternal();
	m_readyForAutocomplete = true;
	RenderInternal(aTitle);

	// markers
	if (mScrollbarMarkers) {
		ImGuiWindow* window = ImGui::GetCurrentWindowRead();
		if (window->ScrollbarY) {
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			ImRect scrollBarRect = ImGui::GetWindowScrollbarRect(window, ImGuiAxis_Y);
			ImGui::PushClipRect(scrollBarRect.Min, scrollBarRect.Max, false);
			int mSelectedLine = mState.mCursorPosition.mLine;

			// current line marker
			if (mSelectedLine != 0) {
				float lineStartY = std::round(scrollBarRect.Min.y + (mSelectedLine - 0.5f) / mLines.size() * scrollBarRect.GetHeight());
				drawList->AddLine(ImVec2(scrollBarRect.Min.x, lineStartY), ImVec2(scrollBarRect.Max.x, lineStartY), (mPalette[(int)PaletteIndex::Default] & 0x00FFFFFFu) | 0x83000000u, 3);
			}

			// changed lines marker
			for (int line : mChangedLines) {
				float lineStartY = std::round(scrollBarRect.Min.y + (float(line) - 0.5f) / mLines.size() * scrollBarRect.GetHeight());
				float lineEndY = std::round(scrollBarRect.Min.y + (float(line+1) - 0.5f) / mLines.size() * scrollBarRect.GetHeight());
				drawList->AddRectFilled(ImVec2(scrollBarRect.Min.x + scrollBarRect.GetWidth() * 0.6f, lineStartY), ImVec2(scrollBarRect.Min.x + scrollBarRect.GetWidth(), lineEndY), 0xFF8CE6F0);
			}

			// error markers
			for (auto& error : mErrorMarkers) {
				float lineStartY = std::round(scrollBarRect.Min.y + (float(error.first) - 0.5f) / mLines.size() * scrollBarRect.GetHeight());
				drawList->AddRectFilled(ImVec2(scrollBarRect.Min.x, lineStartY), ImVec2(scrollBarRect.Min.x + scrollBarRect.GetWidth() * 0.4f, lineStartY + 6.0f), mPalette[(int)PaletteIndex::ErrorMarker]);
			}
			ImGui::PopClipRect();
		}
	}

	if (ImGui::IsMouseClicked(1)) {
		mRightClickPos = ImGui::GetMousePos();
		
		if (ImGui::IsWindowHovered())
			SetCursorPosition(ScreenPosToCoordinates(mRightClickPos));
	}

	bool openBkptConditionWindow = false;
	if (ImGui::BeginPopupContextItem(("##edcontext" + std::string(aTitle)).c_str())) {
		if (mRightClickPos.x - mUICursorPos.x > ImGui::GetStyle().WindowPadding.x + DebugDataSpace) {
			if (ImGui::Selectable("Cut")) { Cut(); }
			if (ImGui::Selectable("Copy")) { Copy(); }
			if (ImGui::Selectable("Paste")) { Paste(); }
		}
		else {
			int line = ScreenPosToCoordinates(mRightClickPos).mLine + 1;

			if (IsDebugging() && ImGui::Selectable("Jump") && OnDebuggerJump) OnDebuggerJump(this, line);
			if (ImGui::Selectable("Breakpoint")) AddBreakpoint(line);
			if (HasBreakpoint(line)) {
				const auto& bkpt = GetBreakpoint(line);
				bool isEnabled = bkpt.mEnabled;
				if (ImGui::Selectable("Condition")) {
					mPopupCondition_Line = line;
					mPopupCondition_Use = bkpt.mUseCondition;
					memcpy(mPopupCondition_Condition, bkpt.mCondition.c_str(), bkpt.mCondition.size());
					mPopupCondition_Condition[std::min<size_t>(511, bkpt.mCondition.size())] = 0;
					openBkptConditionWindow = true;
				}
				if (ImGui::Selectable(isEnabled ? "Disable" : "Enable")) { SetBreakpointEnabled(line, !isEnabled); }
				if (ImGui::Selectable("Delete")) { RemoveBreakpoint(line); }
			}
		}
		ImGui::EndPopup();
	}

	/* FIND TEXT WINDOW */
	if (mFindOpened)
	{
		ImFont* font = ImGui::GetFont();
		ImGui::PopFont();

		ImGui::SetNextWindowPos(ImVec2(mFindOrigin.x + windowWidth - mUICalculateSize(250), mFindOrigin.y + mUICalculateSize(50) * (IsDebugging() && mDebugBar)), ImGuiCond_Always);
		ImGui::BeginChild(("##ted_findwnd" + std::string(aTitle)).c_str(), ImVec2(mUICalculateSize(220), mUICalculateSize(mReplaceOpened ? 90 : 40)), true, ImGuiWindowFlags_NoScrollbar);

		// check for findnext shortcut here...
		ShortcutID curActionID = ShortcutID::Count;
		ImGuiIO& io = ImGui::GetIO();
		auto shift = io.KeyShift;
		auto ctrl = io.ConfigMacOSXBehaviors ? io.KeySuper : io.KeyCtrl;
		auto alt = io.ConfigMacOSXBehaviors ? io.KeyCtrl : io.KeyAlt;
		for (int i = 0; i < m_shortcuts.size(); i++) {
			auto sct = m_shortcuts[i];

			if (sct.Key1 == -1)
				continue;

			////	TODO: SDL keyboard stuff
			////	SDL_Scancode sc1 = SDL_GetScancodeFromKey(sct.Key1);
			////	
			////	if (ImGui::IsKeyPressed(sc1) && ((sct.Key2 != -1 && ImGui::IsKeyPressed(SDL_GetScancodeFromKey(sct.Key2))) || sct.Key2 == -1)) {
			if (false) {
				if (((sct.Ctrl == 0 && !ctrl) || (sct.Ctrl == 1 && ctrl) || (sct.Ctrl == 2)) &&		// ctrl check
					((sct.Alt == 0 && !alt) || (sct.Alt == 1 && alt) || (sct.Alt == 2)) &&			// alt check
					((sct.Shift == 0 && !shift) || (sct.Shift == 1 && shift) || (sct.Shift == 2))) {// shift check
				
					curActionID = (ImTextEdit::ShortcutID)i;
				}
			}
		}
		mFindNext = curActionID == ImTextEdit::ShortcutID::FindNext;

		if (mFindJustOpened) {
			std::string txt = GetSelectedText();
			if (txt.size() > 0)
				strcpy(mFindWord, txt.c_str());
		}

		ImGui::PushItemWidth(mUICalculateSize(-45));
		if (ImGui::InputText(("##ted_findtextbox" + std::string(aTitle)).c_str(), mFindWord, 256, ImGuiInputTextFlags_EnterReturnsTrue) || mFindNext) {
			auto curPos = mState.mCursorPosition;
			size_t cindex = 0;
			for (size_t ln = 0; ln < curPos.mLine; ln++)
				cindex += GetLineCharacterCount(ln) + 1;
			cindex += curPos.mColumn;

			std::string wordLower = mFindWord;
			std::transform(wordLower.begin(), wordLower.end(), wordLower.begin(), ::tolower);

			std::string textSrc = GetText();
			std::transform(textSrc.begin(), textSrc.end(), textSrc.begin(), ::tolower);

			size_t textLoc = textSrc.find(wordLower, cindex);
			if (textLoc == std::string::npos)
				textLoc = textSrc.find(wordLower, 0);


			if (textLoc != std::string::npos) {
				curPos.mLine = curPos.mColumn = 0;
				cindex = 0;
				for (size_t ln = 0; ln < mLines.size(); ln++) {
					int charCount = GetLineCharacterCount(ln) + 1;
					if (cindex + charCount > textLoc) {
						curPos.mLine = ln;
						curPos.mColumn = textLoc - cindex;

						auto& line = mLines[curPos.mLine];
						for (int i = 0; i < line.size(); i++)
							if (line[i].mChar == '\t')
								curPos.mColumn += (mTabSize - 1);

						break;
					}
					else {// just keep adding
						cindex += charCount;
					}
				}


				auto selStart = curPos, selEnd = curPos;
				selEnd.mColumn += strlen(mFindWord);
				SetSelection(curPos, selEnd);
				SetCursorPosition(selEnd);
				mScrollToCursor = true;

				if (!mFindNext)
					ImGui::SetKeyboardFocusHere(0);
			}

			mFindNext = false;
		}
		if (ImGui::IsItemActive())
			mFindFocused = true;
		else
			mFindFocused = false;
		if (mFindJustOpened) {
			ImGui::SetKeyboardFocusHere(0);
			mFindJustOpened = false;
		}
		ImGui::PopItemWidth();

		if (!mReadOnly) {
			ImGui::SameLine();
			if (ImGui::ArrowButton(("##expandFind" + std::string(aTitle)).c_str(), mReplaceOpened ? ImGuiDir_Up : ImGuiDir_Down))
				mReplaceOpened = !mReplaceOpened;
		}

		ImGui::SameLine();
		if (ImGui::Button(("X##" + std::string(aTitle)).c_str()))
			mFindOpened = false;

		if (mReplaceOpened && !mReadOnly) {
			ImGui::PushItemWidth(mUICalculateSize(-45));
			ImGui::NewLine();
			bool shouldReplace = false;
			if (ImGui::InputText(("##ted_replacetb" + std::string(aTitle)).c_str(), mReplaceWord, 256, ImGuiInputTextFlags_EnterReturnsTrue))
				shouldReplace = true;
			if (ImGui::IsItemActive())
				mReplaceFocused = true;
			else
				mReplaceFocused = false;
			ImGui::PopItemWidth();

			ImGui::SameLine();
			if (ImGui::Button((">##replaceOne" + std::string(aTitle)).c_str()) || shouldReplace) {
				if (strlen(mFindWord) > 0) {
					auto curPos = mState.mCursorPosition;
					
					std::string textSrc = GetText();
					if (mReplaceIndex >= textSrc.size())
						mReplaceIndex = 0;
					size_t textLoc = textSrc.find(mFindWord, mReplaceIndex);
					if (textLoc == std::string::npos) {
						mReplaceIndex = 0;
						textLoc = textSrc.find(mFindWord, 0);
					}


					if (textLoc != std::string::npos) {
						curPos.mLine = curPos.mColumn = 0;
						int totalCount = 0;
						for (size_t ln = 0; ln < mLines.size(); ln++) {
							int lineCharCount = GetLineCharacterCount(ln) + 1;
							if (textLoc >= totalCount && textLoc < totalCount + lineCharCount) {
								curPos.mLine = ln;
								curPos.mColumn = textLoc - totalCount;

								auto& line = mLines[curPos.mLine];
								for (int i = 0; i < line.size(); i++)
									if (line[i].mChar == '\t')
										curPos.mColumn += (mTabSize - 1);

								break;
							}
							totalCount += lineCharCount;
						}

						auto selStart = curPos, selEnd = curPos;
						selEnd.mColumn += strlen(mFindWord);
						SetSelection(curPos, selEnd);
						DeleteSelection();
						InsertText(mReplaceWord);
						SetCursorPosition(selEnd);
						mScrollToCursor = true;

						ImGui::SetKeyboardFocusHere(0);

						mReplaceIndex = textLoc + strlen(mReplaceWord);
					}
				}
			}

			ImGui::SameLine();
			if (ImGui::Button((">>##replaceAll" + std::string(aTitle)).c_str())) {
				if (strlen(mFindWord) > 0) {
					auto curPos = mState.mCursorPosition;

					std::string textSrc = GetText();
					size_t textLoc = textSrc.find(mFindWord, 0);

					do {
						if (textLoc != std::string::npos) {
							curPos.mLine = curPos.mColumn = 0;
							int totalCount = 0;
							for (size_t ln = 0; ln < mLines.size(); ln++) {
								int lineCharCount = GetLineCharacterCount(ln) + 1;
								if (textLoc >= totalCount && textLoc < totalCount + lineCharCount) {
									curPos.mLine = ln;
									curPos.mColumn = textLoc - totalCount;

									auto& line = mLines[curPos.mLine];
									for (int i = 0; i < line.size(); i++)
										if (line[i].mChar == '\t')
											curPos.mColumn += (mTabSize - 1);

									break;
								}
								totalCount += lineCharCount;
							}


							auto selStart = curPos, selEnd = curPos;
							selEnd.mColumn += strlen(mFindWord);
							SetSelection(curPos, selEnd);
							DeleteSelection();
							InsertText(mReplaceWord);
							SetCursorPosition(selEnd);
							mScrollToCursor = true;

							ImGui::SetKeyboardFocusHere(0);

							// find next occurance
							textSrc = GetText();
							textLoc += strlen(mReplaceWord);
							textLoc = textSrc.find(mFindWord, textLoc);
						}
					} while (textLoc != std::string::npos);
				}
			}
		}

		ImGui::EndChild();

		ImGui::PushFont(font);

		if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape)))
			mFindOpened = false;
	}

	/* DEBUGGER CONTROLS */
	if (IsDebugging() && mDebugBar)
	{
		ImFont* font = ImGui::GetFont();
		ImGui::PopFont();


		ImGui::SetNextWindowPos(ImVec2(mFindOrigin.x + windowWidth / 2 - mDebugBarWidth / 2, mFindOrigin.y), ImGuiCond_Always);
		ImGui::BeginChild(("##ted_dbgcontrols" + std::string(aTitle)).c_str(), ImVec2(mDebugBarWidth, mDebugBarHeight), true, ImGuiWindowFlags_NoScrollbar);

		ImVec2 dbBarStart = ImGui::GetCursorPos();

		if (ImGui::Button(("Step##ted_dbgstep" + std::string(aTitle)).c_str()) && OnDebuggerAction)
			OnDebuggerAction(this, ImTextEdit::DebugAction::Step);
		ImGui::SameLine(0, 6);

		if (ImGui::Button(("Step In##ted_dbgstepin" + std::string(aTitle)).c_str()) && OnDebuggerAction)
			OnDebuggerAction(this, ImTextEdit::DebugAction::StepInto);
		ImGui::SameLine(0, 6);

		if (ImGui::Button(("Step Out##ted_dbgstepout" + std::string(aTitle)).c_str()) && OnDebuggerAction)
			OnDebuggerAction(this, ImTextEdit::DebugAction::StepOut);
		ImGui::SameLine(0, 6);

		if (ImGui::Button(("Continue##ted_dbgcontinue" + std::string(aTitle)).c_str()) && OnDebuggerAction)
			OnDebuggerAction(this, ImTextEdit::DebugAction::Continue);
		ImGui::SameLine(0, 6);

		if (ImGui::Button(("Stop##ted_dbgstop" + std::string(aTitle)).c_str()) && OnDebuggerAction)
			OnDebuggerAction(this, ImTextEdit::DebugAction::Stop);

		ImVec2 dbBarEnd = ImGui::GetCursorPos();
		mDebugBarHeight = dbBarEnd.y - dbBarStart.y + ImGui::GetStyle().WindowPadding.y * 2.0f;

		ImGui::SameLine(0, 6);

		dbBarEnd = ImGui::GetCursorPos();
		mDebugBarWidth = dbBarEnd.x - dbBarStart.x + ImGui::GetStyle().WindowPadding.x * 2.0f;

		ImGui::EndChild();

		ImGui::PushFont(font);
	}

	if (mHandleKeyboardInputs)
		ImGui::PopAllowKeyboardFocus();

	if (!mIgnoreImGuiChild)
		ImGui::EndChild();
	ImGui::PopStyleColor();
	ImGui::PopStyleVar();

	// breakpoint condition popup
	if (openBkptConditionWindow)
		ImGui::OpenPopup("Condition##condition");

	ImFont* font = ImGui::GetFont();
	ImGui::PopFont();
	ImGui::SetNextWindowSize(ImVec2(430, 175), ImGuiCond_Once);
	if (ImGui::BeginPopupModal("Condition##condition")) {
		ImGui::Checkbox("Use condition", &mPopupCondition_Use);

		if (!mPopupCondition_Use) {
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
		}
		ImGui::InputText("Condition", mPopupCondition_Condition, 512);
		if (!mPopupCondition_Use) {
			ImGui::PopStyleVar();
			ImGui::PopItemFlag();
		}

		if (ImGui::Button("Cancel"))
			ImGui::CloseCurrentPopup();
		ImGui::SameLine();
		if (ImGui::Button("OK")) {
			size_t clen = strlen(mPopupCondition_Condition);
			bool isEmpty = true;
			for (size_t i = 0; i < clen; i++)
				if (!isspace(mPopupCondition_Condition[i])) {
					isEmpty = false;
					break;
				}
			Breakpoint& bkpt = GetBreakpoint(mPopupCondition_Line);
			bkpt.mCondition = mPopupCondition_Condition;
			bkpt.mUseCondition = (mPopupCondition_Use && !isEmpty);

			if (OnBreakpointUpdate)
				OnBreakpointUpdate(this, bkpt.mLine, bkpt.mUseCondition, bkpt.mCondition, bkpt.mEnabled);

			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	ImGui::PushFont(font);

	mWithinRender = false;
}

void ImTextEdit::SetText(const std::string & aText)
{
	mLines.clear();
	mFoldBegin.clear();
	mFoldEnd.clear();
	mFoldSorted = false;

	mLines.emplace_back(Line());
	for (auto chr : aText)
	{
		if (chr == '\r') {
			// ignore the carriage return character
		} else if (chr == '\n')
			mLines.emplace_back(Line());
		else {
			if (chr == '{')
				mFoldBegin.push_back(Coordinates(mLines.size() - 1, mLines.back().size()));
			else if (chr == '}')
				mFoldEnd.push_back(Coordinates(mLines.size() - 1, mLines.back().size()));

			mLines.back().emplace_back(Glyph(chr, PaletteIndex::Default));
		}
	}

	
	mTextChanged = true;
	mScrollToTop = true;

	mUndoBuffer.clear();
	mUndoIndex = 0;

	Colorize();
}

void ImTextEdit::SetTextLines(const std::vector<std::string> & aLines)
{
	mLines.clear();
	mFoldBegin.clear();
	mFoldEnd.clear();
	mFoldSorted = false;

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
			for (size_t j = 0; j < aLine.size(); ++j) {
				if (aLine[j] == '{')
					mFoldBegin.push_back(Coordinates(i, j));
				else if (aLine[j] == '}')
					mFoldEnd.push_back(Coordinates(i, j));

				mLines[i].emplace_back(Glyph(aLine[j], PaletteIndex::Default));
			}
		}
	}

	mTextChanged = true;
	mScrollToTop = true;

	mUndoBuffer.clear();
	mUndoIndex = 0;

	Colorize();
}

void ImTextEdit::EnterCharacter(ImWchar aChar, bool aShift)
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
							line.insert(line.begin(), Glyph(' ', ImTextEdit::PaletteIndex::Background));
					} else
						line.insert(line.begin(), Glyph('\t', ImTextEdit::PaletteIndex::Background));
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
				if (OnContentUpdate != nullptr)
					OnContentUpdate(this);

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
		InsertLine(coord.mLine + 1, coord.mColumn);
		auto& line = mLines[coord.mLine];
		auto& newLine = mLines[coord.mLine + 1];
		auto cindex = GetCharacterIndex(coord);

		int foldOffset = 0;
		for (int i = 0; i < cindex; i++)
			foldOffset -= 1 + (line[i].mChar == '\t') * 3;

		if (mLanguageDefinition.mAutoIndentation && mSmartIndent)
			for (size_t it = 0; it < line.size() && isascii(line[it].mChar) && isblank(line[it].mChar); ++it) {
				newLine.push_back(line[it]);
				foldOffset += 1 + (line[it].mChar == '\t') * 3;
			}

		const size_t whitespaceSize = newLine.size();
		newLine.insert(newLine.end(), line.begin() + cindex, line.end());
		line.erase(line.begin() + cindex, line.begin() + line.size());
		SetCursorPosition(Coordinates(coord.mLine + 1, GetCharacterColumn(coord.mLine + 1, (int)whitespaceSize)));
		u.mAdded = (char)aChar;

		// shift folds
		for (int b = 0; b < mFoldBegin.size(); b++)
			if (mFoldBegin[b].mLine == coord.mLine + 1)
				mFoldBegin[b].mColumn = std::max<int>(0, (mFoldBegin[b].mColumn + foldOffset) + (mFoldBegin[b].mColumn != coord.mColumn));
		for (int b = 0; b < mFoldEnd.size(); b++)
			if (mFoldEnd[b].mLine == coord.mLine + 1)
				mFoldEnd[b].mColumn = std::max<int>(0, (mFoldEnd[b].mColumn + foldOffset) + (mFoldEnd[b].mColumn != coord.mColumn));
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

					// remove fold information if needed
					if (line[cindex].mChar == '{')
						for (int fl = 0; fl < mFoldBegin.size(); fl++)
							if (mFoldBegin[fl].mLine == coord.mLine && mFoldBegin[fl].mColumn == coord.mColumn) {
								mFoldBegin.erase(mFoldBegin.begin() + fl);
								mFoldSorted = false;
								break;
							}
					if (line[cindex].mChar == '}')
						for (int fl = 0; fl < mFoldEnd.size(); fl++)
							if (mFoldEnd[fl].mLine == coord.mLine && mFoldEnd[fl].mColumn == coord.mColumn) {
								mFoldEnd.erase(mFoldEnd.begin() + fl);
								mFoldSorted = false;
								break;
							}

					line.erase(line.begin() + cindex);
				}
			}

			// move the folds if necessary
			int foldOffset = 0;
			if (buf[0] == '\t') foldOffset = mTabSize - (coord.mColumn - (coord.mColumn / mTabSize) * mTabSize);
			else foldOffset = strlen(buf);

			int foldColumn = GetCharacterColumn(coord.mLine, cindex);
			for (int b = 0; b < mFoldBegin.size(); b++) 
				if (mFoldBegin[b].mLine == coord.mLine && mFoldBegin[b].mColumn >= foldColumn)
					mFoldBegin[b].mColumn += foldOffset;
			for (int b = 0; b < mFoldEnd.size(); b++)
				if (mFoldEnd[b].mLine == coord.mLine && mFoldEnd[b].mColumn >= foldColumn)
					mFoldEnd[b].mColumn += foldOffset;

			// insert text
			for (auto p = buf; *p != '\0'; p++, ++cindex) {
				if (*p == '{') {
					mFoldBegin.push_back(Coordinates(coord.mLine, foldColumn));
					mFoldSorted = false;
				} else if (*p == '}') {
					mFoldEnd.push_back(Coordinates(coord.mLine, foldColumn));
					mFoldSorted = false;
				}

				line.insert(line.begin() + cindex, Glyph(*p, PaletteIndex::Default));
			}
			u.mAdded = buf;

			SetCursorPosition(Coordinates(coord.mLine, GetCharacterColumn(coord.mLine, cindex)));
		}
		else
			return;
	}
		
	// active suggestions
	if (mActiveAutocomplete && aChar <= 127 && (isalpha(aChar) || aChar == '_')) {
		m_requestAutocomplete = true;
		m_readyForAutocomplete = false;
	}

	if (mScrollbarMarkers) {
		bool changeExists = false;
		for (int i = 0; i < mChangedLines.size(); i++) {
			if (mChangedLines[i] == mState.mCursorPosition.mLine) {
				changeExists = true;
				break;
			}
		}
		if (!changeExists)
			mChangedLines.push_back(mState.mCursorPosition.mLine);
	}

	mTextChanged = true;
	if (OnContentUpdate != nullptr)
		OnContentUpdate(this);

	u.mAddedEnd = GetActualCursorCoordinates();
	u.mAfter = mState;

	AddUndo(u);

	Colorize(coord.mLine - 1, 3);
	EnsureCursorVisible();

	// function tooltip
	if (mFunctionDeclarationTooltipEnabled) {
		if (aChar == '(') {
			auto curPos = GetCorrectCursorPosition();
			std::string obj = GetWordAt(curPos);
			mOpenFunctionDeclarationTooltip(obj, curPos);
		} else if (aChar == ',') {
			auto curPos = GetCorrectCursorPosition();
			curPos.mColumn--;

			const auto& line = mLines[curPos.mLine];
			std::string obj = "";
			int weight = 0;

			for (; curPos.mColumn > 0; curPos.mColumn--) {
				if (line[curPos.mColumn].mChar == '(') {
					if (weight == 0) {
						obj = GetWordAt(curPos);
						break;
					}

					weight--;
				}
				if (line[curPos.mColumn].mChar == ')')
					weight++;
			}

			if (!obj.empty())
				mOpenFunctionDeclarationTooltip(obj, curPos);
		}
	}

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

void ImTextEdit::SetReadOnly(bool aValue)
{
	mReadOnly = aValue;
}

void ImTextEdit::SetColorizerEnable(bool aValue)
{
	mColorizerEnabled = aValue;
}

ImTextEdit::Coordinates ImTextEdit::GetCorrectCursorPosition()
{
	auto curPos = GetCursorPosition();

	if (curPos.mLine >= 0 && curPos.mLine <= GetCursorPosition().mLine) {
		for (int c = 0; c < std::min<int>(curPos.mLine, mLines[curPos.mLine].size()); c++) {
			if (mLines[curPos.mLine][c].mChar == '\t')
				curPos.mColumn -= (GetTabSize() - 1);
		}
	}

	return curPos;
}

void ImTextEdit::SetCursorPosition(const Coordinates & aPosition)
{
	if (mState.mCursorPosition != aPosition)
	{
		mState.mCursorPosition = aPosition;
		mCursorPositionChanged = true;
		EnsureCursorVisible();
	}
}

void ImTextEdit::SetSelectionStart(const Coordinates & aPosition)
{
	mState.mSelectionStart = SanitizeCoordinates(aPosition);
	if (mState.mSelectionStart > mState.mSelectionEnd)
		std::swap(mState.mSelectionStart, mState.mSelectionEnd);
}

void ImTextEdit::SetSelectionEnd(const Coordinates & aPosition)
{
	mState.mSelectionEnd = SanitizeCoordinates(aPosition);
	if (mState.mSelectionStart > mState.mSelectionEnd)
		std::swap(mState.mSelectionStart, mState.mSelectionEnd);
}

void ImTextEdit::SetSelection(const Coordinates & aStart, const Coordinates & aEnd, SelectionMode aMode)
{
	auto oldSelStart = mState.mSelectionStart;
	auto oldSelEnd = mState.mSelectionEnd;

	mState.mSelectionStart = SanitizeCoordinates(aStart);
	mState.mSelectionEnd = SanitizeCoordinates(aEnd);
	if (mState.mSelectionStart > mState.mSelectionEnd)
		std::swap(mState.mSelectionStart, mState.mSelectionEnd);

	switch (aMode)
	{
	case ImTextEdit::SelectionMode::Normal:
		break;
	case ImTextEdit::SelectionMode::Word:
	{
		mState.mSelectionStart = FindWordStart(mState.mSelectionStart);
		if (!IsOnWordBoundary(mState.mSelectionEnd))
			mState.mSelectionEnd = FindWordEnd(FindWordStart(mState.mSelectionEnd));
		break;
	}
	case ImTextEdit::SelectionMode::Line:
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

	// update mReplaceIndex
	mReplaceIndex = 0;
	for (size_t ln = 0; ln < mState.mCursorPosition.mLine; ln++)
		mReplaceIndex += GetLineCharacterCount(ln) + 1;
	mReplaceIndex += mState.mCursorPosition.mColumn;
}

void ImTextEdit::InsertText(const std::string& aValue, bool indent)
{
	InsertText(aValue.c_str(), indent);
}

void ImTextEdit::InsertText(const char* aValue, bool indent)
{
	if (aValue == nullptr)
		return;

	auto pos = GetActualCursorCoordinates();
	auto start = std::min<Coordinates>(pos, mState.mSelectionStart);
	int totalLines = pos.mLine - start.mLine;

	totalLines += InsertTextAt(pos, aValue, indent);

	SetSelection(pos, pos);
	SetCursorPosition(pos);
	Colorize(start.mLine - 1, totalLines + 2);
}

void ImTextEdit::DeleteSelection()
{
	assert(mState.mSelectionEnd >= mState.mSelectionStart);

	if (mState.mSelectionEnd == mState.mSelectionStart)
		return;

	DeleteRange(mState.mSelectionStart, mState.mSelectionEnd);

	SetSelection(mState.mSelectionStart, mState.mSelectionStart);
	SetCursorPosition(mState.mSelectionStart);
	Colorize(mState.mSelectionStart.mLine, 1);
}

void ImTextEdit::MoveUp(int aAmount, bool aSelect)
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

void ImTextEdit::MoveDown(int aAmount, bool aSelect)
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

void ImTextEdit::MoveLeft(int aAmount, bool aSelect, bool aWordMode)
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

void ImTextEdit::MoveRight(int aAmount, bool aSelect, bool aWordMode)
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

void ImTextEdit::MoveTop(bool aSelect)
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

void ImTextEdit::MoveBottom(bool aSelect)
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

void ImTextEdit::MoveHome(bool aSelect)
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

void ImTextEdit::MoveEnd(bool aSelect)
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

void ImTextEdit::Delete()
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

			// move folds
			for (int i = 0; i < mFoldBegin.size(); i++)
				if (mFoldBegin[i].mLine == pos.mLine + 1) {
					mFoldBegin[i].mLine = std::max<int>(0, mFoldBegin[i].mLine - 1);
					mFoldBegin[i].mColumn += GetCharacterColumn(pos.mLine, line.size());
				}
			for (int i = 0; i < mFoldEnd.size(); i++)
				if (mFoldEnd[i].mLine == pos.mLine + 1) {
					mFoldEnd[i].mLine = std::max<int>(0, mFoldEnd[i].mLine - 1);
					mFoldEnd[i].mColumn += GetCharacterColumn(pos.mLine, line.size());
				}


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

			mRemoveFolds(u.mRemovedStart, u.mRemovedEnd);

			auto d = UTF8CharLength(line[cindex].mChar);
			while (d-- > 0 && cindex < (int)line.size())
				line.erase(line.begin() + cindex);
		}

		if (mScrollbarMarkers) {
			bool changeExists = false;
			for (int i = 0; i < mChangedLines.size(); i++) {
				if (mChangedLines[i] == mState.mCursorPosition.mLine) {
					changeExists = true;
					break;
				}
			}
			if (!changeExists)
				mChangedLines.push_back(mState.mCursorPosition.mLine);
		}

		mTextChanged = true;
		if (OnContentUpdate != nullptr)
			OnContentUpdate(this);

		Colorize(pos.mLine, 1);
	}

	u.mAfter = mState;
	AddUndo(u);
}

void ImTextEdit::Backspace()
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

			// error markers
			ErrorMarkers etmp;
			for (auto& i : mErrorMarkers)
				etmp.insert(ErrorMarkers::value_type(i.first - 1 == mState.mCursorPosition.mLine ? i.first - 1 : i.first, i.second));
			mErrorMarkers = std::move(etmp);

			// shift folds
			for (int b = 0; b < mFoldBegin.size(); b++)
				if (mFoldBegin[b].mLine == mState.mCursorPosition.mLine) {
					mFoldBegin[b].mLine = std::max<int>(0, mFoldBegin[b].mLine - 1);
					mFoldBegin[b].mColumn = mFoldBegin[b].mColumn + prevSize;
				}
			for (int b = 0; b < mFoldEnd.size(); b++)
				if (mFoldEnd[b].mLine == mState.mCursorPosition.mLine) {
					mFoldEnd[b].mLine = std::max<int>(0, mFoldEnd[b].mLine - 1);
					mFoldEnd[b].mColumn = mFoldEnd[b].mColumn + prevSize;
				}

			RemoveLine(mState.mCursorPosition.mLine);
			--mState.mCursorPosition.mLine;
			mState.mCursorPosition.mColumn = prevSize;
		} else {
			auto& line = mLines[mState.mCursorPosition.mLine];
			auto cindex = GetCharacterIndex(pos) - 1;
			auto cend = cindex + 1;
			while (cindex > 0 && IsUTFSequence(line[cindex].mChar))
				--cindex;

			//if (cindex > 0 && UTF8CharLength(line[cindex].mChar) > 1)
			//	--cindex;

			int actualLoc = pos.mColumn;
			for (int i = 0; i < line.size(); i++) {
				if (line[i].mChar == '\t')
					actualLoc -= GetTabSize() - 1;
			}

			if (mCompleteBraces && actualLoc > 0 && actualLoc < line.size()) {
				if ((line[actualLoc - 1].mChar == '(' && line[actualLoc].mChar == ')') || (line[actualLoc - 1].mChar == '{' && line[actualLoc].mChar == '}') || (line[actualLoc - 1].mChar == '[' && line[actualLoc].mChar == ']'))
					Delete();
			}

			u.mRemovedStart = u.mRemovedEnd = GetActualCursorCoordinates();

			while (cindex < line.size() && cend-- > cindex) {
				uint8_t chVal = line[cindex].mChar;

				int remColumn = 0;
				for (int i = 0; i < cindex && i < line.size(); i++) {
					if (line[i].mChar == '\t') {
						int tabSize = remColumn - (remColumn / mTabSize) * mTabSize;
						remColumn += mTabSize - tabSize;
					} else
						remColumn++;
				}
				int remSize = mState.mCursorPosition.mColumn - remColumn;

				u.mRemoved += line[cindex].mChar;
				u.mRemovedStart.mColumn -= remSize;

				line.erase(line.begin() + cindex);

				mState.mCursorPosition.mColumn -= remSize;
			}
		
			mRemoveFolds(u.mRemovedStart, u.mRemovedEnd);
		}

		if (mScrollbarMarkers) {
			bool changeExists = false;
			for (int i = 0; i < mChangedLines.size(); i++) {
				if (mChangedLines[i] == mState.mCursorPosition.mLine) {
					changeExists = true;
					break;
				}
			}
			if (!changeExists)
				mChangedLines.push_back(mState.mCursorPosition.mLine);
		}

		mTextChanged = true;
		if (OnContentUpdate != nullptr)
			OnContentUpdate(this);

		EnsureCursorVisible();
		Colorize(mState.mCursorPosition.mLine, 1);
	}

	u.mAfter = mState;
	AddUndo(u);

	// autocomplete
	if (mActiveAutocomplete && mACOpened) {
		m_requestAutocomplete = true;
		m_readyForAutocomplete = false;
	}
}

void ImTextEdit::SelectWordUnderCursor()
{
	auto c = GetCursorPosition();
	SetSelection(FindWordStart(c), FindWordEnd(c));
}

void ImTextEdit::SelectAll()
{
	SetSelection(Coordinates(0, 0), Coordinates((int)mLines.size(), 0));
}

bool ImTextEdit::HasSelection() const
{
	return mState.mSelectionEnd > mState.mSelectionStart;
}

void ImTextEdit::SetShortcut(ImTextEdit::ShortcutID id, Shortcut s) {
	m_shortcuts[(int)id].Key1 = s.Key1;
	m_shortcuts[(int)id].Key2 = s.Key2;
	if (m_shortcuts[(int)id].Ctrl != 2)
		m_shortcuts[(int)id].Ctrl = s.Ctrl;
	if (m_shortcuts[(int)id].Shift != 2)
		m_shortcuts[(int)id].Shift = s.Shift;
	if (m_shortcuts[(int)id].Alt != 2)
		m_shortcuts[(int)id].Alt = s.Alt;
}

void ImTextEdit::Copy()
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

void ImTextEdit::Cut()
{
	if (IsReadOnly()) {
		Copy();
	} else if (HasSelection()) {
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

void ImTextEdit::Paste()
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

		InsertText(clipText, mAutoindentOnPaste);

		u.mAddedEnd = GetActualCursorCoordinates();
		u.mAfter = mState;
		AddUndo(u);
	}
}

bool ImTextEdit::CanUndo()
{
	return !IsReadOnly() && mUndoIndex > 0;
}

bool ImTextEdit::CanRedo()
{
	return !IsReadOnly() && mUndoIndex < (int)mUndoBuffer.size();
}

void ImTextEdit::Undo(int aSteps)
{
	while (CanUndo() && aSteps-- > 0)
		mUndoBuffer[--mUndoIndex].Undo(this);
}

void ImTextEdit::Redo(int aSteps)
{
	while (CanRedo() && aSteps-- > 0)
		mUndoBuffer[mUndoIndex++].Redo(this);
}

std::vector<std::string> ImTextEdit::GetRelevantExpressions(int line)
{
	std::vector<std::string> ret;
	line--;

	if (line < 0 || line >= mLines.size() || (mLanguageDefinition.mName != "HLSL" && mLanguageDefinition.mName != "GLSL"))
		return ret;

	std::string expr = "";
	for (int i = 0; i < mLines[line].size(); i++)
		expr += mLines[line][i].mChar;

	enum class TokenType {
		Identifier,
		Operator,
		Number,
		Parenthesis,
		Comma,
		Semicolon
	};
	struct Token {
		TokenType Type;
		std::string Content;
	};

	char buffer[512] = { 0 };
	int bufferLoc = 0;
	std::vector<Token> tokens;

	// convert expression into list of tokens
	const char* e = expr.c_str();
	while (*e != 0) {
		if (*e == '*' || *e == '/' || *e == '+' || *e == '-' || *e == '%' || *e == '&' || *e == '|' || *e == '=' || *e == '(' || *e == ')' || *e == ',' || *e == ';' || *e == '<' || *e == '>') {
			if (bufferLoc != 0)
				tokens.push_back({ TokenType::Identifier, std::string(buffer) });

			memset(buffer, 0, 512);
			bufferLoc = 0;

			if (*e == '(' || *e == ')')
				tokens.push_back({ TokenType::Parenthesis, std::string(e, 1) });
			else if (*e == ',')
				tokens.push_back({ TokenType::Comma, "," });
			else if (*e == ';')
				tokens.push_back({ TokenType::Semicolon, ";" });
			else
				tokens.push_back({ TokenType::Operator, std::string(e, 1) });
		} else if (*e == '{' || *e == '}')
			break;
		else if (*e == '\n' || *e == '\r' || *e == ' ' || *e == '\t') {
			// empty the buffer if needed
			if (bufferLoc != 0) {
				tokens.push_back({ TokenType::Identifier, std::string(buffer) });

				memset(buffer, 0, 512);
				bufferLoc = 0;
			}
		} else {
			buffer[bufferLoc] = *e;
			bufferLoc++;
		}
		e++;
	}

	// empty the buffer
	if (bufferLoc != 0)
		tokens.push_back({ TokenType::Identifier, std::string(buffer) });

	// some "post processing"
	int multilineComment = 0;
	for (int i = 0; i < tokens.size(); i++) {
		if (tokens[i].Type == TokenType::Identifier) {
			if (tokens[i].Content.size() > 0) {
				if (tokens[i].Content[0] == '.' || isdigit(tokens[i].Content[0]))
					tokens[i].Type = TokenType::Number;
				else if (tokens[i].Content == "true" || tokens[i].Content == "false")
					tokens[i].Type = TokenType::Number;
			}
		} else if (i != 0 && tokens[i].Type == TokenType::Operator) {
			if (tokens[i - 1].Type == TokenType::Operator) {
				// comment
				if (tokens[i].Content == "/" && tokens[i - 1].Content == "/") {
					tokens.erase(tokens.begin() + i - 1, tokens.end());
					break;
				} else if (tokens[i - 1].Content == "/" && tokens[i].Content == "*")
					multilineComment = i - 1;
				else if (tokens[i - 1].Content == "*" && tokens[i].Content == "/") {
					tokens.erase(tokens.begin() + multilineComment, tokens.begin() + i + 1);
					i -= (i - multilineComment) - 1;
					multilineComment = 0;
					continue;
				} else {
					// &&, <=, ...
					tokens[i - 1].Content += tokens[i].Content;
					tokens.erase(tokens.begin() + i);
					i--;
					continue;
				}
			}
		}
	}

	// 1. get all the identifiers (highest priority)
	for (int i = 0; i < tokens.size(); i++) {
		if (tokens[i].Type == TokenType::Identifier) {
			if (i == tokens.size() - 1 || tokens[i + 1].Content != "(")
				if (std::count(ret.begin(), ret.end(), tokens[i].Content) == 0)
					ret.push_back(tokens[i].Content);
		}
	}

	// 2. get all the function calls, their arguments and expressions
	std::stack<std::string> funcStack;
	std::stack<std::string> argStack;
	std::string exprBuffer = "";
	int exprParenthesis = 0;
	int forSection = -1;
	for (int i = 0; i < tokens.size(); i++) {
		if ((forSection == -1 || forSection == 1) && tokens[i].Type == TokenType::Identifier && i + 1 < tokens.size() && tokens[i + 1].Content == "(") {
			if (tokens[i].Content == "if" || tokens[i].Content == "for" || tokens[i].Content == "while") {
				if (tokens[i].Content == "for")
					forSection = 0;
				else
					i++; // skip '('
				continue;
			}

			funcStack.push(tokens[i].Content + "(");
			argStack.push("");
			i++;
			continue;
		} else if ((forSection == -1 || forSection == 1) && (tokens[i].Type == TokenType::Comma || tokens[i].Content == ")") && !argStack.empty() && !funcStack.empty()) {
			funcStack.top() += argStack.top().substr(0, argStack.top().size() - 1) + tokens[i].Content;

			if (tokens[i].Content == ")") {
				std::string topFunc = funcStack.top();
				funcStack.pop();

				if (!argStack.top().empty())
					ret.push_back(argStack.top().substr(0, argStack.top().size() - 1));
				argStack.pop();
				if (!argStack.empty())
					argStack.top() += topFunc + " ";

				ret.push_back(topFunc);

				if (funcStack.empty())
					exprBuffer += topFunc + " ";
			} else if (tokens[i].Type == TokenType::Comma) {
				funcStack.top() += " ";
				ret.push_back(argStack.top().substr(0, argStack.top().size() - 1));
				argStack.top() = "";
			}
		} else if (tokens[i].Type == TokenType::Semicolon) {
			if (forSection != -1) {
				if (forSection == 1 && !exprBuffer.empty()) {
					ret.push_back(exprBuffer);
					exprBuffer.clear();
					exprParenthesis = 0;
				}
				forSection++;
			}
		} else if (forSection == -1 || forSection == 1) {
			if (tokens[i].Content == "(")
				exprParenthesis++;
			else if (tokens[i].Content == ")")
				exprParenthesis--;

			if (!argStack.empty())
				argStack.top() += tokens[i].Content + " ";
			else if (exprParenthesis < 0) {
				if (!exprBuffer.empty())
					ret.push_back(exprBuffer.substr(0, exprBuffer.size() - 1));
				exprBuffer.clear();
				exprParenthesis = 0;
			} else if (tokens[i].Type == TokenType::Operator && (tokens[i].Content.size() >= 2 || tokens[i].Content == "=")) {
				if (!exprBuffer.empty())
					ret.push_back(exprBuffer.substr(0, exprBuffer.size() - 1));
				exprBuffer.clear();
				exprParenthesis = 0;
			} else {
				bool isKeyword = false;
				for (const auto& kwd : mLanguageDefinition.mKeywords) {
					if (kwd == tokens[i].Content) {
						isKeyword = true;
						break;
					}
				}
				if (!isKeyword)
					exprBuffer += tokens[i].Content + " ";
			}
		}
	}

	if (!exprBuffer.empty())
		ret.push_back(exprBuffer.substr(0, exprBuffer.size() - 1));

	// remove duplicates, numbers & keywords
	for (int i = 0; i < ret.size(); i++) {
		std::string r = ret[i];
		bool eraseR = false;

		// empty or duplicate
		if (ret.empty() || std::count(ret.begin(), ret.begin() + i, r) > 0)
			eraseR = true;

		// boolean
		if (r == "true" || r == "false")
			eraseR = true;

		// number
		bool isNumber = true;
		for (int i = 0; i < r.size(); i++)
			if (!isdigit(r[i]) && r[i] != '.' && r[i] != 'f') {
				isNumber = false;
				break;
			}
		if (isNumber)
			eraseR = true;

		// keyword
		if (!eraseR) {
			for (const auto& ident : mLanguageDefinition.mIdentifiers)
				if (ident.first == r) {
					eraseR = true;
					break;
				}

			for (const auto& kwd : mLanguageDefinition.mKeywords)
				if (kwd == r) {
					eraseR = true;
					break;
				}
		}

		// delete it from the array
		if (eraseR) {
			ret.erase(ret.begin() + i);
			i--;
			continue;
		}
	}

	return ret;
}

const ImTextEdit::Palette & ImTextEdit::GetDarkPalette()
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
		0xff0000ff, // Breakpoint
		0xffffffff, // Breakpoint outline
		0xFF1DD8FF, // Current line indicator
		0xFF696969, // Current line indicator outline
		0xff707000, // Line number
		0x40000000, // Current line fill
		0x40808080, // Current line fill (inactive)
		0x40a0a0a0, // Current line edge
		0xff33ffff, // Error message
		0xffffffff, // BreakpointDisabled
		0xffaaaaaa, // UserFunction
		0xffb0c94e, // UserType
		0xffaaaaaa, // UniformType
		0xffaaaaaa, // GlobalVariable
		0xffaaaaaa, // LocalVariable
		0xff888888	// FunctionArgument
	} };
	return p;
}

const ImTextEdit::Palette & ImTextEdit::GetLightPalette()
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
		0x80DFBF80, // Selection
		0xa00010ff, // ErrorMarker
		0xff0000ff, // Breakpoint
		0xff000000, // Breakpoint outline
		0xFF1DD8FF, // Current line indicator
		0xFF696969, // Current line indicator outline
		0xff505000, // Line number
		0x20000000, // Current line fill
		0x20808080, // Current line fill (inactive)
		0x30000000, // Current line edge
		0xff3333ff, // Error message
		0xffffffff, // BreakpointDisabled
		0xff404040, // UserFunction
		0xffb0912b, // UserType
		0xff404040, // UniformType
		0xff404040, // GlobalVariable
		0xff404040, // LocalVariable
		0xff606060	// FunctionArgument
	} };
	return p;
}

const ImTextEdit::Palette & ImTextEdit::GetRetroBluePalette()
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
		0xff0000ff, // Breakpoint
		0xffffffff, // Breakpoint outline
		0xFF1DD8FF, // Current line indicator
		0xff808000, // Line number
		0x40000000, // Current line fill
		0x40808080, // Current line fill (inactive)
		0x40000000, // Current line edge
		0xffffff00, // Error message
		0xffffffff, // BreakpointDisabled
		0xff00ffff, // UserFunction
		0xff00ffff, // UserType
		0xff00ffff, // UniformType
		0xff00ffff, // GlobalVariable
		0xff00ffff, // LocalVariable
		0xff00ffff	// FunctionArgument
	} };
	return p;
}


std::string ImTextEdit::GetText() const
{
	return GetText(Coordinates(), Coordinates((int)mLines.size(), 0));
}

void ImTextEdit::GetTextLines(std::vector<std::string>& result) const
{
	result.reserve(mLines.size());

	for (auto & line : mLines)
	{
		std::string text;
		text.resize(line.size());

		for (size_t i = 0; i < line.size(); ++i)
			text[i] = line[i].mChar;

		result.emplace_back(std::move(text));
	}
}

std::string ImTextEdit::GetSelectedText() const
{
	return GetText(mState.mSelectionStart, mState.mSelectionEnd);
}

std::string ImTextEdit::GetCurrentLineText()const
{
	auto lineLength = GetLineMaxColumn(mState.mCursorPosition.mLine);
	return GetText(
		Coordinates(mState.mCursorPosition.mLine, 0),
		Coordinates(mState.mCursorPosition.mLine, lineLength));
}

void ImTextEdit::ProcessInputs()
{
}

void ImTextEdit::Colorize(int aFromLine, int aLines)
{
	int toLine = aLines == -1 ? (int)mLines.size() : std::min<int>((int)mLines.size(), aFromLine + aLines);
	mColorRangeMin = std::min<int>(mColorRangeMin, aFromLine);
	mColorRangeMax = std::max<int>(mColorRangeMax, toLine);
	mColorRangeMin = std::max<int>(0, mColorRangeMin);
	mColorRangeMax = std::max<int>(mColorRangeMin, mColorRangeMax);
	mCheckComments = true;
}

void ImTextEdit::ColorizeRange(int aFromLine, int aToLine)
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
						else {
							bool found = false;
							
							// functions, arguments, local variables
							for (const auto& func : mACFunctions) {
								if (strcmp(func.first.c_str(), id.c_str()) == 0) {
									token_color = PaletteIndex::UserFunction;
									found = true;
									break;
								}

								if (i >= func.second.LineStart - 3 && i <= func.second.LineEnd + 1) {
									for (const auto& arg : func.second.Arguments) {
										if (strcmp(arg.Name.c_str(), id.c_str()) == 0) {
											token_color = PaletteIndex::FunctionArgument;
											found = true;
											break;
										}
									}
									if (!found) {
										for (const auto& loc : func.second.Locals) {
											if (strcmp(loc.Name.c_str(), id.c_str()) == 0) {
												token_color = PaletteIndex::LocalVariable;
												found = true;
												break;
											}
										}
										if (found) break;
									} else
										break;
								}
							}

							// uniforms
							if (!found) {
								for (const auto& unif : mACUniforms) {
									if (strcmp(unif.Name.c_str(), id.c_str()) == 0) {
										token_color = PaletteIndex::UniformVariable;
										found = true;
										break;
									}
								}
							}

							// globals
							if (!found) {
								for (const auto& glob : mACGlobals) {
									if (strcmp(glob.Name.c_str(), id.c_str()) == 0) {
										token_color = PaletteIndex::GlobalVariable;
										found = true;
										break;
									}
								}
							}

							// user types
							if (!found) {
								for (const auto& userType : mACUserTypes) {
									if (strcmp(userType.first.c_str(), id.c_str()) == 0) {
										token_color = PaletteIndex::UserType;
										found = true;
										break;
									}
								}
							}
						}
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

void ImTextEdit::ColorizeInternal()
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

float ImTextEdit::TextDistanceToLineStart(const Coordinates& aFrom) const
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

void ImTextEdit::EnsureCursorVisible()
{
	if (!mWithinRender)
	{
		mScrollToCursor = true;
		return;
	}

	float scrollX = ImGui::GetScrollX();
	float scrollY = ImGui::GetScrollY();

	auto height = ImGui::GetWindowHeight();
	auto width = mWindowWidth;
	
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
	if (pos.mColumn < left)
		ImGui::SetScrollX(std::max(0.0f, len + mTextStart - 11 * mCharAdvance.x));
	if (len + mTextStart > (right - 4) * mCharAdvance.x)
		ImGui::SetScrollX(std::max(0.0f, len + mTextStart + 4 * mCharAdvance.x - width));
}
void ImTextEdit::SetCurrentLineIndicator(int line, bool displayBar)
{
	mDebugCurrentLine = line;
	mDebugCurrentLineUpdated = line > 0;
	mDebugBar = displayBar;
}

int ImTextEdit::GetPageSize() const
{
	auto height = ImGui::GetWindowHeight() - 20.0f;
	return (int)floor(height / mCharAdvance.y);
}

ImTextEdit::UndoRecord::UndoRecord(
	const std::string& aAdded,
	const ImTextEdit::Coordinates aAddedStart,
	const ImTextEdit::Coordinates aAddedEnd,
	const std::string& aRemoved,
	const ImTextEdit::Coordinates aRemovedStart,
	const ImTextEdit::Coordinates aRemovedEnd,
	ImTextEdit::EditorState& aBefore,
	ImTextEdit::EditorState& aAfter)
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

void ImTextEdit::UndoRecord::Undo(ImTextEdit * aEditor)
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

void ImTextEdit::UndoRecord::Redo(ImTextEdit * aEditor)
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

const ImTextEdit::LanguageDefinition& ImTextEdit::LanguageDefinition::CPlusPlus()
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

const ImTextEdit::LanguageDefinition& ImTextEdit::LanguageDefinition::HLSL()
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
			"half1x3","half2x3","half3x3","half4x3","half1x4","half2x4","half3x4","half4x4",
			"SHADERED_WEB", "SHADERED_DESKTOP", "SHADERED_VERSION"
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
void ImTextEdit::LanguageDefinition::m_HLSLDocumentation(Identifiers& idents)
{
	/* SOURCE: https://docs.microsoft.com/en-us/windows/desktop/direct3dhlsl/dx-graphics-hlsl-intrinsic-functions */

	idents.insert(std::make_pair("abort", Identifier("Terminates the current draw or dispatch call being executed.")));
	idents.insert(std::make_pair("abs", Identifier("Absolute value (per component).")));
	idents.insert(std::make_pair("acos", Identifier("Returns the arccosine of each component of x.")));
	idents.insert(std::make_pair("all", Identifier("Test if all components of x are nonzero.")));
	idents.insert(std::make_pair("AllMemoryBarrier", Identifier("Blocks execution of all threads in a group until all memory accesses have been completed.")));
	idents.insert(std::make_pair("AllMemoryBarrierWithGroupSync", Identifier("Blocks execution of all threads in a group until all memory accesses have been completed and all threads in the group have reached this call.")));
	idents.insert(std::make_pair("any", Identifier("Test if any component of x is nonzero.")));
	idents.insert(std::make_pair("asdouble", Identifier("Reinterprets a cast value into a double.")));
	idents.insert(std::make_pair("asfloat", Identifier("Convert the input type to a float.")));
	idents.insert(std::make_pair("asin", Identifier("Returns the arcsine of each component of x.")));
	idents.insert(std::make_pair("asint", Identifier("Convert the input type to an integer.")));
	idents.insert(std::make_pair("asuint", Identifier("Convert the input type to an unsigned integer.")));
	idents.insert(std::make_pair("atan", Identifier("Returns the arctangent of x.")));
	idents.insert(std::make_pair("atan2", Identifier("Returns the arctangent of of two values (x,y).")));
	idents.insert(std::make_pair("ceil", Identifier("Returns the smallest integer which is greater than or equal to x.")));
	idents.insert(std::make_pair("CheckAccessFullyMapped", Identifier("Determines whether all values from a Sample or Load operation accessed mapped tiles in a tiled resource.")));
	idents.insert(std::make_pair("clamp", Identifier("Clamps x to the range [min, max].")));
	idents.insert(std::make_pair("clip", Identifier("Discards the current pixel, if any component of x is less than zero.")));
	idents.insert(std::make_pair("cos", Identifier("Returns the cosine of x.")));
	idents.insert(std::make_pair("cosh", Identifier("Returns the hyperbolic cosine of x.")));
	idents.insert(std::make_pair("countbits", Identifier("Counts the number of bits (per component) in the input integer.")));
	idents.insert(std::make_pair("cross", Identifier("Returns the cross product of two 3D vectors.")));
	idents.insert(std::make_pair("D3DCOLORtoUBYTE4", Identifier("Swizzles and scales components of the 4D vector x to compensate for the lack of UBYTE4 support in some hardware.")));
	idents.insert(std::make_pair("ddx", Identifier("Returns the partial derivative of x with respect to the screen-space x-coordinate.")));
	idents.insert(std::make_pair("ddx_coarse", Identifier("Computes a low precision partial derivative with respect to the screen-space x-coordinate.")));
	idents.insert(std::make_pair("ddx_fine", Identifier("Computes a high precision partial derivative with respect to the screen-space x-coordinate.")));
	idents.insert(std::make_pair("ddy", Identifier("Returns the partial derivative of x with respect to the screen-space y-coordinate.")));
	idents.insert(std::make_pair("ddy_coarse", Identifier("Returns the partial derivative of x with respect to the screen-space y-coordinate.")));
	idents.insert(std::make_pair("ddy_fine", Identifier("Computes a high precision partial derivative with respect to the screen-space y-coordinate.")));
	idents.insert(std::make_pair("degrees", Identifier("Converts x from radians to degrees.")));
	idents.insert(std::make_pair("determinant", Identifier("Returns the determinant of the square matrix m.")));
	idents.insert(std::make_pair("DeviceMemoryBarrier", Identifier("Blocks execution of all threads in a group until all device memory accesses have been completed.")));
	idents.insert(std::make_pair("DeviceMemoryBarrierWithGroupSync", Identifier("Blocks execution of all threads in a group until all device memory accesses have been completed and all threads in the group have reached this call.")));
	idents.insert(std::make_pair("distance", Identifier("Returns the distance between two points.")));
	idents.insert(std::make_pair("dot", Identifier("Returns the dot product of two vectors.")));
	idents.insert(std::make_pair("dst", Identifier("Calculates a distance vector.")));
	idents.insert(std::make_pair("errorf", Identifier("Submits an error message to the information queue.")));
	idents.insert(std::make_pair("EvaluateAttributeAtCentroid", Identifier("Evaluates at the pixel centroid.")));
	idents.insert(std::make_pair("EvaluateAttributeAtSample", Identifier("Evaluates at the indexed sample location.")));
	idents.insert(std::make_pair("EvaluateAttributeSnapped", Identifier("Evaluates at the pixel centroid with an offset.")));
	idents.insert(std::make_pair("exp", Identifier("Returns the base-e exponent.")));
	idents.insert(std::make_pair("exp2", Identifier("Base 2 exponent(per component).")));
	idents.insert(std::make_pair("f16tof32", Identifier("Converts the float16 stored in the low-half of the uint to a float.")));
	idents.insert(std::make_pair("f32tof16", Identifier("Converts an input into a float16 type.")));
	idents.insert(std::make_pair("faceforward", Identifier("Returns -n * sign(dot(i, ng)).")));
	idents.insert(std::make_pair("firstbithigh", Identifier("Gets the location of the first set bit starting from the highest order bit and working downward, per component.")));
	idents.insert(std::make_pair("firstbitlow", Identifier("Returns the location of the first set bit starting from the lowest order bit and working upward, per component.")));
	idents.insert(std::make_pair("floor", Identifier("Returns the greatest integer which is less than or equal to x.")));
	idents.insert(std::make_pair("fma", Identifier("Returns the double-precision fused multiply-addition of a * b + c.")));
	idents.insert(std::make_pair("fmod", Identifier("Returns the floating point remainder of x/y.")));
	idents.insert(std::make_pair("frac", Identifier("Returns the fractional part of x.")));
	idents.insert(std::make_pair("frexp", Identifier("Returns the mantissa and exponent of x.")));
	idents.insert(std::make_pair("fwidth", Identifier("Returns abs(ddx(x)) + abs(ddy(x))")));
	idents.insert(std::make_pair("GetRenderTargetSampleCount", Identifier("Returns the number of render-target samples.")));
	idents.insert(std::make_pair("GetRenderTargetSamplePosition", Identifier("Returns a sample position (x,y) for a given sample index.")));
	idents.insert(std::make_pair("GroupMemoryBarrier", Identifier("Blocks execution of all threads in a group until all group shared accesses have been completed.")));
	idents.insert(std::make_pair("GroupMemoryBarrierWithGroupSync", Identifier("Blocks execution of all threads in a group until all group shared accesses have been completed and all threads in the group have reached this call.")));
	idents.insert(std::make_pair("InterlockedAdd", Identifier("Performs a guaranteed atomic add of value to the dest resource variable.")));
	idents.insert(std::make_pair("InterlockedAnd", Identifier("Performs a guaranteed atomic and.")));
	idents.insert(std::make_pair("InterlockedCompareExchange", Identifier("Atomically compares the input to the comparison value and exchanges the result.")));
	idents.insert(std::make_pair("InterlockedCompareStore", Identifier("Atomically compares the input to the comparison value.")));
	idents.insert(std::make_pair("InterlockedExchange", Identifier("Assigns value to dest and returns the original value.")));
	idents.insert(std::make_pair("InterlockedMax", Identifier("Performs a guaranteed atomic max.")));
	idents.insert(std::make_pair("InterlockedMin", Identifier("Performs a guaranteed atomic min.")));
	idents.insert(std::make_pair("InterlockedOr", Identifier("Performs a guaranteed atomic or.")));
	idents.insert(std::make_pair("InterlockedXor", Identifier("Performs a guaranteed atomic xor.")));
	idents.insert(std::make_pair("isfinite", Identifier("Returns true if x is finite, false otherwise.")));
	idents.insert(std::make_pair("isinf", Identifier("Returns true if x is +INF or -INF, false otherwise.")));
	idents.insert(std::make_pair("isnan", Identifier("Returns true if x is NAN or QNAN, false otherwise.")));
	idents.insert(std::make_pair("ldexp", Identifier("Returns x * 2exp")));
	idents.insert(std::make_pair("length", Identifier("Returns the length of the vector v.")));
	idents.insert(std::make_pair("lerp", Identifier("Returns x + s(y - x).")));
	idents.insert(std::make_pair("lit", Identifier("Returns a lighting vector (ambient, diffuse, specular, 1)")));
	idents.insert(std::make_pair("log", Identifier("Returns the base-e logarithm of x.")));
	idents.insert(std::make_pair("log10", Identifier("Returns the base-10 logarithm of x.")));
	idents.insert(std::make_pair("log2", Identifier("Returns the base - 2 logarithm of x.")));
	idents.insert(std::make_pair("mad", Identifier("Performs an arithmetic multiply/add operation on three values.")));
	idents.insert(std::make_pair("max", Identifier("Selects the greater of x and y.")));
	idents.insert(std::make_pair("min", Identifier("Selects the lesser of x and y.")));
	idents.insert(std::make_pair("modf", Identifier("Splits the value x into fractional and integer parts.")));
	idents.insert(std::make_pair("msad4", Identifier("Compares a 4-byte reference value and an 8-byte source value and accumulates a vector of 4 sums.")));
	idents.insert(std::make_pair("mul", Identifier("Performs matrix multiplication using x and y.")));
	idents.insert(std::make_pair("noise", Identifier("Generates a random value using the Perlin-noise algorithm.")));
	idents.insert(std::make_pair("normalize", Identifier("Returns a normalized vector.")));
	idents.insert(std::make_pair("pow", Identifier("Returns x^n.")));
	idents.insert(std::make_pair("printf", Identifier("Submits a custom shader message to the information queue.")));
	idents.insert(std::make_pair("Process2DQuadTessFactorsAvg", Identifier("Generates the corrected tessellation factors for a quad patch.")));
	idents.insert(std::make_pair("Process2DQuadTessFactorsMax", Identifier("Generates the corrected tessellation factors for a quad patch.")));
	idents.insert(std::make_pair("Process2DQuadTessFactorsMin", Identifier("Generates the corrected tessellation factors for a quad patch.")));
	idents.insert(std::make_pair("ProcessIsolineTessFactors", Identifier("Generates the rounded tessellation factors for an isoline.")));
	idents.insert(std::make_pair("ProcessQuadTessFactorsAvg", Identifier("Generates the corrected tessellation factors for a quad patch.")));
	idents.insert(std::make_pair("ProcessQuadTessFactorsMax", Identifier("Generates the corrected tessellation factors for a quad patch.")));
	idents.insert(std::make_pair("ProcessQuadTessFactorsMin", Identifier("Generates the corrected tessellation factors for a quad patch.")));
	idents.insert(std::make_pair("ProcessTriTessFactorsAvg", Identifier("Generates the corrected tessellation factors for a tri patch.")));
	idents.insert(std::make_pair("ProcessTriTessFactorsMax", Identifier("Generates the corrected tessellation factors for a tri patch.")));
	idents.insert(std::make_pair("ProcessTriTessFactorsMin", Identifier("Generates the corrected tessellation factors for a tri patch.")));
	idents.insert(std::make_pair("radians", Identifier("Converts x from degrees to radians.")));
	idents.insert(std::make_pair("rcp", Identifier("Calculates a fast, approximate, per-component reciprocal.")));
	idents.insert(std::make_pair("reflect", Identifier("Returns a reflection vector.")));
	idents.insert(std::make_pair("refract", Identifier("Returns the refraction vector.")));
	idents.insert(std::make_pair("reversebits", Identifier("Reverses the order of the bits, per component.")));
	idents.insert(std::make_pair("round", Identifier("Rounds x to the nearest integer")));
	idents.insert(std::make_pair("rsqrt", Identifier("Returns 1 / sqrt(x)")));
	idents.insert(std::make_pair("saturate", Identifier("Clamps x to the range [0, 1]")));
	idents.insert(std::make_pair("sign", Identifier("Computes the sign of x.")));
	idents.insert(std::make_pair("sin", Identifier("Returns the sine of x")));
	idents.insert(std::make_pair("sincos", Identifier("Returns the sineand cosine of x.")));
	idents.insert(std::make_pair("sinh", Identifier("Returns the hyperbolic sine of x")));
	idents.insert(std::make_pair("smoothstep", Identifier("Returns a smooth Hermite interpolation between 0 and 1.")));
	idents.insert(std::make_pair("sqrt", Identifier("Square root (per component)")));
	idents.insert(std::make_pair("step", Identifier("Returns (x >= a) ? 1 : 0")));
	idents.insert(std::make_pair("tan", Identifier("Returns the tangent of x")));
	idents.insert(std::make_pair("tanh", Identifier("Returns the hyperbolic tangent of x")));
	idents.insert(std::make_pair("tex1D", Identifier("1D texture lookup.")));
	idents.insert(std::make_pair("tex1Dbias", Identifier("1D texture lookup with bias.")));
	idents.insert(std::make_pair("tex1Dgrad", Identifier("1D texture lookup with a gradient.")));
	idents.insert(std::make_pair("tex1Dlod", Identifier("1D texture lookup with LOD.")));
	idents.insert(std::make_pair("tex1Dproj", Identifier("1D texture lookup with projective divide.")));
	idents.insert(std::make_pair("tex2D", Identifier("2D texture lookup.")));
	idents.insert(std::make_pair("tex2Dbias", Identifier("2D texture lookup with bias.")));
	idents.insert(std::make_pair("tex2Dgrad", Identifier("2D texture lookup with a gradient.")));
	idents.insert(std::make_pair("tex2Dlod", Identifier("2D texture lookup with LOD.")));
	idents.insert(std::make_pair("tex2Dproj", Identifier("2D texture lookup with projective divide.")));
	idents.insert(std::make_pair("tex3D", Identifier("3D texture lookup.")));
	idents.insert(std::make_pair("tex3Dbias", Identifier("3D texture lookup with bias.")));
	idents.insert(std::make_pair("tex3Dgrad", Identifier("3D texture lookup with a gradient.")));
	idents.insert(std::make_pair("tex3Dlod", Identifier("3D texture lookup with LOD.")));
	idents.insert(std::make_pair("tex3Dproj", Identifier("3D texture lookup with projective divide.")));
	idents.insert(std::make_pair("texCUBE", Identifier("Cube texture lookup.")));
	idents.insert(std::make_pair("texCUBEbias", Identifier("Cube texture lookup with bias.")));
	idents.insert(std::make_pair("texCUBEgrad", Identifier("Cube texture lookup with a gradient.")));
	idents.insert(std::make_pair("texCUBElod", Identifier("Cube texture lookup with LOD.")));
	idents.insert(std::make_pair("texCUBEproj", Identifier("Cube texture lookup with projective divide.")));
	idents.insert(std::make_pair("transpose", Identifier("Returns the transpose of the matrix m.")));
	idents.insert(std::make_pair("trunc", Identifier("Truncates floating-point value(s) to integer value(s)")));
}

const ImTextEdit::LanguageDefinition& ImTextEdit::LanguageDefinition::GLSL()
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
			"usampler2DMSArray", "samplerCubeArray", "samplerCubeArrayShadow", "isamplerCubeArray", "usamplerCubeArray",
			"SHADERED_WEB", "SHADERED_DESKTOP", "SHADERED_VERSION", "shared", "writeonly", "readonly", "image2D", "image1D", "image3D"
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
void ImTextEdit::LanguageDefinition::m_GLSLDocumentation(Identifiers& idents)
{
	/* SOURCE: https://docs.microsoft.com/en-us/windows/desktop/direct3dhlsl/dx-graphics-hlsl-intrinsic-functions */

	idents.insert(std::make_pair("radians", Identifier("genType radians(genType degrees)\nConverts x from degrees to radians.")));
	idents.insert(std::make_pair("degrees", Identifier("genType degrees(genType radians)\nConverts x from radians to degrees.")));
	idents.insert(std::make_pair("sin", Identifier("genType sin(genType angle)\nReturns the sine of x")));
	idents.insert(std::make_pair("cos", Identifier("genType cos(genType angle)\nReturns the cosine of x.")));
	idents.insert(std::make_pair("tan", Identifier("genType tan(genType angle)\nReturns the tangent of x")));
	idents.insert(std::make_pair("asin", Identifier("genType asin(genType x)\nReturns the arcsine of each component of x.")));
	idents.insert(std::make_pair("acos", Identifier("genType acos(genType x)\nReturns the arccosine of each component of x.")));
	idents.insert(std::make_pair("atan", Identifier("genType atan(genType y, genType x)\ngenType atan(genType y_over_x)\nReturns the arctangent of x.")));
	idents.insert(std::make_pair("sinh", Identifier("genType sinh(genType x)\nReturns the hyperbolic sine of x")));
	idents.insert(std::make_pair("cosh", Identifier("genType cosh(genType x)\nReturns the hyperbolic cosine of x.")));
	idents.insert(std::make_pair("tanh", Identifier("genType tanh(genType x)\nReturns the hyperbolic tangent of x")));
	idents.insert(std::make_pair("asinh", Identifier("genType asinh(genType x)\nReturns the arc hyperbolic sine of x")));
	idents.insert(std::make_pair("acosh", Identifier("genType acosh(genType x)\nReturns the arc hyperbolic cosine of x.")));
	idents.insert(std::make_pair("atanh", Identifier("genType atanh(genType x)\nReturns the arc hyperbolic tangent of x")));
	idents.insert(std::make_pair("pow", Identifier("genType pow(genType x, genType n)\nReturns x^n.")));
	idents.insert(std::make_pair("exp", Identifier("genType exp(genType x)\nReturns the base-e exponent.")));
	idents.insert(std::make_pair("exp2", Identifier("genType exp2(genType x)\nBase 2 exponent(per component).")));
	idents.insert(std::make_pair("log", Identifier("genType log(genType x)\nReturns the base-e logarithm of x.")));
	idents.insert(std::make_pair("log2", Identifier("genType log2(genType x)\nReturns the base - 2 logarithm of x.")));
	idents.insert(std::make_pair("sqrt", Identifier("genType sqrt(genType x)\ngenDType sqrt(genDType x)\nSquare root (per component).")));
	idents.insert(std::make_pair("inversesqrt", Identifier("genType inversesqrt(genType x)\ngenDType inversesqrt(genDType x)\nReturns rcp(sqrt(x)).")));
	idents.insert(std::make_pair("abs", Identifier("genType abs(genType x)\ngenIType abs(genIType x)\ngenDType abs(genDType x)\nAbsolute value (per component).")));
	idents.insert(std::make_pair("sign", Identifier("genType sign(genType x)\ngenIType sign(genIType x)\ngenDType sign(genDType x)\nComputes the sign of x.")));
	idents.insert(std::make_pair("floor", Identifier("genType floor(genType x)\ngenDType floor(genDType x)\nReturns the greatest integer which is less than or equal to x.")));
	idents.insert(std::make_pair("trunc", Identifier("genType trunc(genType x)\ngenDType trunc(genDType x)\nTruncates floating-point value(s) to integer value(s)")));
	idents.insert(std::make_pair("round", Identifier("genType round(genType x)\ngenDType round(genDType x)\nRounds x to the nearest integer")));
	idents.insert(std::make_pair("roundEven", Identifier("genType roundEven(genType x)\ngenDType roundEven(genDType x)\nReturns a value equal to the nearest integer to x. A fractional part of 0.5 will round toward the nearest even integer.")));
	idents.insert(std::make_pair("ceil", Identifier("genType ceil(genType x)\ngenDType ceil(genDType x)\nReturns the smallest integer which is greater than or equal to x.")));
	idents.insert(std::make_pair("fract", Identifier("genType fract(genType x)\ngenDType fract(genDType x)\nReturns the fractional part of x.")));
	idents.insert(std::make_pair("mod", Identifier("genType mod(genType x, float y)\ngenType mod(genType x, genType y)\ngenDType mod(genDType x, double y)\ngenDType mod(genDType x, genDType y)\nModulus. Returns x – y ∗ floor (x/y).")));
	idents.insert(std::make_pair("modf", Identifier("genType modf(genType x, out genType i)\ngenDType modf(genDType x, out genDType i)\nSplits the value x into fractional and integer parts.")));
	idents.insert(std::make_pair("max", Identifier("genType max(genType x, genType y)\ngenType max(genType x, float y)\nSelects the greater of x and y.")));
	idents.insert(std::make_pair("min", Identifier("genType min(genType x, genType y)\ngenType min(genType x, float y)\nSelects the lesser of x and y.")));
	idents.insert(std::make_pair("clamp", Identifier("genType clamp(genType x, genType minVal, genType maxVal)\ngenType clamp(genType x, float minVal, float maxVal)\nClamps x to the range [min, max].")));
	idents.insert(std::make_pair("mix", Identifier("genType mix(genType x, genType y, genType a)\ngenType mix(genType x, genType y, float a)\nReturns x*(1-a)+y*a.")));
	idents.insert(std::make_pair("isinf", Identifier("genBType isinf(genType x)\ngenBType isinf(genDType x)\nReturns true if x is +INF or -INF, false otherwise.")));
	idents.insert(std::make_pair("isnan", Identifier("genBType isnan(genType x)\ngenBType isnan(genDType x)\nReturns true if x is NAN or QNAN, false otherwise.")));
	idents.insert(std::make_pair("smoothstep", Identifier("genType smoothstep(genType edge0, genType edge1, genType x)\ngenType smoothstep(float edge0, float edge1, genType x)\nReturns a smooth Hermite interpolation between 0 and 1.")));
	idents.insert(std::make_pair("step", Identifier("genType step(genType edge, genType x)\ngenType step(float edge, genType x)\nReturns (x >= a) ? 1 : 0")));
	idents.insert(std::make_pair("floatBitsToInt", Identifier("genIType floatBitsToInt(genType x)\nReturns a signed or unsigned integer value representing the encoding of a floating-point value. The floatingpoint value's bit-level representation is preserved.")));
	idents.insert(std::make_pair("floatBitsToUint", Identifier("genUType floatBitsToUint(genType x)\nReturns a signed or unsigned integer value representing the encoding of a floating-point value. The floatingpoint value's bit-level representation is preserved.")));
	idents.insert(std::make_pair("intBitsToFloat", Identifier("genType intBitsToFloat(genIType x)\nReturns a floating-point value corresponding to a signed or unsigned integer encoding of a floating-point value.")));
	idents.insert(std::make_pair("uintBitsToFloat", Identifier("genType uintBitsToFloat(genUType x)\nReturns a floating-point value corresponding to a signed or unsigned integer encoding of a floating-point value.")));
	idents.insert(std::make_pair("fmod", Identifier("Returns the floating point remainder of x/y.")));
	idents.insert(std::make_pair("fma", Identifier("genType fma(genType a, genType b, genType c)\nReturns the double-precision fused multiply-addition of a * b + c.")));
	idents.insert(std::make_pair("ldexp", Identifier("genType ldexp(genType x, genIType exp)\nReturns x * 2exp")));
	idents.insert(std::make_pair("packUnorm2x16", Identifier("uint packUnorm2x16(vec2 v)\nFirst, converts each component of the normalized floating - point value v into 8 or 16bit integer values. Then, the results are packed into the returned 32bit unsigned integer.")));
	idents.insert(std::make_pair("packUnorm4x8", Identifier("uint packUnorm4x8(vec4 v)\nFirst, converts each component of the normalized floating - point value v into 8 or 16bit integer values. Then, the results are packed into the returned 32bit unsigned integer.")));
	idents.insert(std::make_pair("packSnorm4x8", Identifier("uint packUnorm4x8(vec4 v)\nFirst, converts each component of the normalized floating - point value v into 8 or 16bit integer values. Then, the results are packed into the returned 32bit unsigned integer.")));
	idents.insert(std::make_pair("unpackUnorm2x16", Identifier("vec2 unpackUnorm2x16(uint p)\nFirst, unpacks a single 32bit unsigned integer p into a pair of 16bit unsigned integers, four 8bit unsigned integers, or four 8bit signed integers.Then, each component is converted to a normalized floating point value to generate the returned two or four component vector.")));
	idents.insert(std::make_pair("unpackUnorm4x8", Identifier("vec4 unpackUnorm4x8(uint p)\nFirst, unpacks a single 32bit unsigned integer p into a pair of 16bit unsigned integers, four 8bit unsigned integers, or four 8bit signed integers.Then, each component is converted to a normalized floating point value to generate the returned two or four component vector.")));
	idents.insert(std::make_pair("unpackSnorm4x8", Identifier("vec4 unpackSnorm4x8(uint p)\nFirst, unpacks a single 32bit unsigned integer p into a pair of 16bit unsigned integers, four 8bit unsigned integers, or four 8bit signed integers.Then, each component is converted to a normalized floating point value to generate the returned two or four component vector.")));
	idents.insert(std::make_pair("packDouble2x32", Identifier("double packDouble2x32(uvec2 v)\nReturns a double-precision value obtained by packing the components of v into a 64-bit value.")));
	idents.insert(std::make_pair("unpackDouble2x32", Identifier("uvec2 unpackDouble2x32(double d)\nReturns a two-component unsigned integer vector representation of v.")));
	idents.insert(std::make_pair("length", Identifier("float length(genType x)\nReturns the length of the vector v.")));
	idents.insert(std::make_pair("distance", Identifier("float distance(genType p0, genType p1)\nReturns the distance between two points.")));
	idents.insert(std::make_pair("dot", Identifier("float dot(genType x, genType y)\nReturns the dot product of two vectors.")));
	idents.insert(std::make_pair("cross", Identifier("vec3 cross(vec3 x, vec3 y)\nReturns the cross product of two 3D vectors.")));
	idents.insert(std::make_pair("normalize", Identifier("genType normalize(genType v)\nReturns a normalized vector.")));
	idents.insert(std::make_pair("faceforward", Identifier("genType faceforward(genType N, genType I, genType Nref)\nReturns -n * sign(dot(i, ng)).")));
	idents.insert(std::make_pair("reflect", Identifier("genType reflect(genType I, genType N)\nReturns a reflection vector.")));
	idents.insert(std::make_pair("refract", Identifier("genType refract(genType I, genType N, float eta)\nReturns the refraction vector.")));
	idents.insert(std::make_pair("matrixCompMult", Identifier("mat matrixCompMult(mat x, mat y)\nMultiply matrix x by matrix y component-wise.")));
	idents.insert(std::make_pair("outerProduct", Identifier("Linear algebraic matrix multiply c * r.")));
	idents.insert(std::make_pair("transpose", Identifier("mat transpose(mat m)\nReturns the transpose of the matrix m.")));
	idents.insert(std::make_pair("determinant", Identifier("float determinant(mat m)\nReturns the determinant of the square matrix m.")));
	idents.insert(std::make_pair("inverse", Identifier("mat inverse(mat m)\nReturns a matrix that is the inverse of m.")));
	idents.insert(std::make_pair("lessThan", Identifier("bvec lessThan(vec x, vec y)\nReturns the component-wise compare of x < y")));
	idents.insert(std::make_pair("lessThanEqual", Identifier("bvec lessThanEqual(vec x, vec y)\nReturns the component-wise compare of x <= y")));
	idents.insert(std::make_pair("greaterThan", Identifier("bvec greaterThan(vec x, vec y)\nReturns the component-wise compare of x > y")));
	idents.insert(std::make_pair("greaterThanEqual", Identifier("bvec greaterThanEqual(vec x, vec y)\nReturns the component-wise compare of x >= y")));
	idents.insert(std::make_pair("equal", Identifier("bvec equal(vec x, vec y)\nReturns the component-wise compare of x == y")));
	idents.insert(std::make_pair("notEqual", Identifier("bvec notEqual(vec x, vec y)\nReturns the component-wise compare of x != y")));
	idents.insert(std::make_pair("any", Identifier("bool any(bvec x)\nTest if any component of x is nonzero.")));
	idents.insert(std::make_pair("all", Identifier("bool all(bvec x)\nTest if all components of x are nonzero.")));
	idents.insert(std::make_pair("not", Identifier("bvec not(bvec x)\nReturns the component-wise logical complement of x.")));
	idents.insert(std::make_pair("uaddCarry", Identifier("genUType uaddCarry(genUType x, genUType y, out genUType carry)\nAdds 32bit unsigned integer x and y, returning the sum modulo 2^32.")));
	idents.insert(std::make_pair("usubBorrow", Identifier("genUType usubBorrow(genUType x, genUType y, out genUType borrow)\nSubtracts the 32bit unsigned integer y from x, returning the difference if non-negatice, or 2^32 plus the difference otherwise.")));
	idents.insert(std::make_pair("umulExtended", Identifier("void umulExtended(genUType x, genUType y, out genUType msb, out genUType lsb)\nMultiplies 32bit integers x and y, producing a 64bit result.")));
	idents.insert(std::make_pair("imulExtended", Identifier("void imulExtended(genIType x, genIType y, out genIType msb, out genIType lsb)\nMultiplies 32bit integers x and y, producing a 64bit result.")));
	idents.insert(std::make_pair("bitfieldExtract", Identifier("genIType bitfieldExtract(genIType value, int offset, int bits)\ngenUType bitfieldExtract(genUType value, int offset, int bits)\nExtracts bits [offset, offset + bits - 1] from value, returning them in the least significant bits of the result.")));
	idents.insert(std::make_pair("bitfieldInsert", Identifier("genIType bitfieldInsert(genIType base, genIType insert, int offset, int bits)\ngenUType bitfieldInsert(genUType base, genUType insert, int offset, int bits)\nReturns the insertion the bits leas-significant bits of insert into base")));
	idents.insert(std::make_pair("bitfieldReverse", Identifier("genIType bitfieldReverse(genIType value)\ngenUType bitfieldReverse(genUType value)\nReturns the reversal of the bits of value.")));
	idents.insert(std::make_pair("bitCount", Identifier("genIType bitCount(genIType value)\ngenUType bitCount(genUType value)\nReturns the number of bits set to 1 in the binary representation of value.")));
	idents.insert(std::make_pair("findLSB", Identifier("genIType findLSB(genIType value)\ngenUType findLSB(genUType value)\nReturns the bit number of the least significant bit set to 1 in the binary representation of value.")));
	idents.insert(std::make_pair("findMSB", Identifier("genIType findMSB(genIType value)\ngenUType findMSB(genUType value)\nReturns the bit number of the most significant bit in the binary representation of value.")));
	idents.insert(std::make_pair("textureSize", Identifier("ivecX textureSize(gsamplerXD sampler, int lod)\nReturns the dimensions of level lod  (if present) for the texture bound to sample.")));
	idents.insert(std::make_pair("textureQueryLod", Identifier("vec2 textureQueryLod(gsamplerXD sampler, vecX P)\nReturns the mipmap array(s) that would be accessed in the x component of the return value.")));
	idents.insert(std::make_pair("texture", Identifier("gvec4 texture(gsamplerXD sampler, vecX P, [float bias])\nUse the texture coordinate P to do a texture lookup in the texture currently bound to sampler.")));
	idents.insert(std::make_pair("textureProj", Identifier("Do a texture lookup with projection.")));
	idents.insert(std::make_pair("textureLod", Identifier("gvec4 textureLod(gsamplerXD sampler, vecX P, float lod)\nDo a texture lookup as in texture but with explicit LOD.")));
	idents.insert(std::make_pair("textureOffset", Identifier("gvec4 textureOffset(gsamplerXD sampler, vecX P, ivecX offset, [float bias])\nDo a texture lookup as in texture but with offset added to the (u,v,w) texel coordinates before looking up each texel.")));
	idents.insert(std::make_pair("texelFetch", Identifier("gvec4 texelFetch(gsamplerXD sampler, ivecX P, int lod)\nUse integer texture coordinate P to lookup a single texel from sampler.")));
	idents.insert(std::make_pair("texelFetchOffset", Identifier("gvec4 texelFetchOffset(gsamplerXD sampler, ivecX P, int lod, int offset)\nFetch a single texel as in texelFetch offset by offset.")));
	idents.insert(std::make_pair("textureProjLod", Identifier("Do a projective texture lookup with explicit LOD.")));
	idents.insert(std::make_pair("textureLodOffset", Identifier("gvec4 textureLodOffset(gsamplerXD sampler, vecX P, float lod, ivecX offset)\nDo an offset texture lookup with explicit LOD.")));
	idents.insert(std::make_pair("textureProjLodOffset", Identifier("Do an offset projective texture lookup with explicit LOD.")));
	idents.insert(std::make_pair("textureGrad", Identifier("gvec4 textureGrad(gsamplerXD sampler, vecX P, vecX dPdx, vecX dPdy)\nDo a texture lookup as in texture but with explicit gradients.")));
	idents.insert(std::make_pair("textureGradOffset", Identifier("gvec4 textureGradOffset(gsamplerXD sampler, vecX P, vecX dPdx, vecX dPdy, ivecX offset)\nDo a texture lookup with both explicit gradient and offset, as described in textureGrad and textureOffset.")));
	idents.insert(std::make_pair("textureProjGrad", Identifier("Do a texture lookup both projectively and with explicit gradient.")));
	idents.insert(std::make_pair("textureProjGradOffset", Identifier("Do a texture lookup both projectively and with explicit gradient as well as with offset.")));
	idents.insert(std::make_pair("textureGather", Identifier("gvec4 textureGather(gsampler2D sampler, vec2 P, [int comp])\nGathers four texels from a texture")));
	idents.insert(std::make_pair("textureGatherOffset", Identifier("gvec4 textureGatherOffset(gsampler2D sampler, vec2 P, ivec2 offset, [int comp])\nGathers four texels from a texture with offset.")));
	idents.insert(std::make_pair("textureGatherOffsets", Identifier("gvec4 textureGatherOffsets(gsampler2D sampler, vec2 P, ivec2 offsets[4], [int comp])\nGathers four texels from a texture with an array of offsets.")));
	idents.insert(std::make_pair("texture1D", Identifier("1D texture lookup.")));
	idents.insert(std::make_pair("texture1DLod", Identifier("1D texture lookup with LOD.")));
	idents.insert(std::make_pair("texture1DProj", Identifier("1D texture lookup with projective divide.")));
	idents.insert(std::make_pair("texture1DProjLod", Identifier("1D texture lookup with projective divide and with LOD.")));
	idents.insert(std::make_pair("texture2D", Identifier("2D texture lookup.")));
	idents.insert(std::make_pair("texture2DLod", Identifier("2D texture lookup with LOD.")));
	idents.insert(std::make_pair("texture2DProj", Identifier("2D texture lookup with projective divide.")));
	idents.insert(std::make_pair("texture2DProjLod", Identifier("2D texture lookup with projective divide and with LOD.")));
	idents.insert(std::make_pair("texture3D", Identifier("3D texture lookup.")));
	idents.insert(std::make_pair("texture3DLod", Identifier("3D texture lookup with LOD.")));
	idents.insert(std::make_pair("texture3DProj", Identifier("3D texture lookup with projective divide.")));
	idents.insert(std::make_pair("texture3DProjLod", Identifier("3D texture lookup with projective divide and with LOD.")));
	idents.insert(std::make_pair("textureCube", Identifier("Cube texture lookup.")));
	idents.insert(std::make_pair("textureCubeLod", Identifier("Cube texture lookup with LOD.")));
	idents.insert(std::make_pair("shadow1D", Identifier("1D texture lookup.")));
	idents.insert(std::make_pair("shadow1DLod", Identifier("1D texture lookup with LOD.")));
	idents.insert(std::make_pair("shadow1DProj", Identifier("1D texture lookup with projective divide.")));
	idents.insert(std::make_pair("shadow1DProjLod", Identifier("1D texture lookup with projective divide and with LOD.")));
	idents.insert(std::make_pair("shadow2D", Identifier("2D texture lookup.")));
	idents.insert(std::make_pair("shadow2DLod", Identifier("2D texture lookup with LOD.")));
	idents.insert(std::make_pair("shadow2DProj", Identifier("2D texture lookup with projective divide.")));
	idents.insert(std::make_pair("shadow2DProjLod", Identifier("2D texture lookup with projective divide and with LOD.")));

	idents.insert(std::make_pair("dFdx", Identifier("genType dFdx(genType p)\nReturns the partial derivative of x with respect to the screen-space x-coordinate.")));
	idents.insert(std::make_pair("dFdy", Identifier("genType dFdy(genType p)\nReturns the partial derivative of x with respect to the screen-space y-coordinate.")));
	idents.insert(std::make_pair("fwidth", Identifier("genType fwidth(genType p)\nReturns abs(ddx(x)) + abs(ddy(x))")));
	idents.insert(std::make_pair("interpolateAtCentroid", Identifier("Return the value of the input varying interpolant sampled at a location inside the both the pixel and the primitive being processed.")));
	idents.insert(std::make_pair("interpolateAtSample", Identifier("Return the value of the input varying interpolant at the location of sample number sample.")));
	idents.insert(std::make_pair("interpolateAtOffset", Identifier("Return the value of the input varying interpolant sampled at an offset from the center of the pixel specified by offset.")));
	idents.insert(std::make_pair("noise1", Identifier("Generates a random value")));
	idents.insert(std::make_pair("noise2", Identifier("Generates a random value")));
	idents.insert(std::make_pair("noise3", Identifier("Generates a random value")));
	idents.insert(std::make_pair("noise4", Identifier("Generates a random value")));
	idents.insert(std::make_pair("EmitStreamVertex", Identifier("void EmitStreamVertex(int stream)\nEmit the current values of output variables to the current output primitive on stream stream.")));
	idents.insert(std::make_pair("EndStreamPrimitive", Identifier("void EndStreamPrimitive(int stream)\nCompletes the current output primitive on stream stream and starts a new one.")));
	idents.insert(std::make_pair("EmitVertex", Identifier("void EmitVertex()\nEmit the current values to the current output primitive.")));
	idents.insert(std::make_pair("EndPrimitive", Identifier("void EndPrimitive()\nCompletes the current output primitive and starts a new one.")));
	idents.insert(std::make_pair("barrier", Identifier("void barrier()\nSynchronize execution of multiple shader invocations")));
	idents.insert(std::make_pair("groupMemoryBarrier", Identifier("void groupMemoryBarrier()\nControls the ordering of memory transaction issued shader invocation relative to a work group")));
	idents.insert(std::make_pair("memoryBarrier", Identifier("uint memoryBarrier()\nControls the ordering of memory transactions issued by a single shader invocation")));
	idents.insert(std::make_pair("memoryBarrierAtomicCounter", Identifier("void memoryBarrierAtomicCounter()\nControls the ordering of operations on atomic counters issued by a single shader invocation")));
	idents.insert(std::make_pair("memoryBarrierBuffer", Identifier("void memoryBarrierBuffer()\nControls the ordering of operations on buffer variables issued by a single shader invocation")));
	idents.insert(std::make_pair("memoryBarrierImage", Identifier("void memoryBarrierImage()\nControls the ordering of operations on image variables issued by a single shader invocation")));
	idents.insert(std::make_pair("memoryBarrierShared", Identifier("void memoryBarrierShared()\nControls the ordering of operations on shared variables issued by a single shader invocation")));

	idents.insert(std::make_pair("atomicAdd", Identifier("int atomicAdd(inout int mem, int data)\nuint atomicAdd(inout uint mem, uint data)\nPerform an atomic addition to a variable")));
	idents.insert(std::make_pair("atomicAnd", Identifier("int atomicAnd(inout int mem, int data)\nuint atomicAnd(inout uint mem, uint data)\nPerform an atomic logical AND operation to a variable")));
	idents.insert(std::make_pair("atomicCompSwap", Identifier("int atomicCompSwap(inout int mem, uint compare, uint data)\nuint atomicCompSwap(inout uint mem, uint compare, uint data)\nPerform an atomic compare-exchange operation to a variable")));
	idents.insert(std::make_pair("atomicCounter", Identifier("uint atomicCounter(atomic_uint c)\nReturn the current value of an atomic counter")));
	idents.insert(std::make_pair("atomicCounterDecrement", Identifier("uint atomicCounterDecrement(atomic_uint c)\nAtomically decrement a counter and return its new value")));
	idents.insert(std::make_pair("atomicCounterIncrement", Identifier("uint atomicCounterIncrement(atomic_uint c)\nAtomically increment a counter and return the prior value")));
	idents.insert(std::make_pair("atomicExchange", Identifier("int atomicExchange(inout int mem, int data)\nuint atomicExchange(inout uint mem, uint data)\nPerform an atomic exchange operation to a variable ")));
	idents.insert(std::make_pair("atomicMax", Identifier("int atomicMax(inout int mem, int data)\nuint atomicMax(inout uint mem, uint data)\nPerform an atomic max operation to a variable")));
	idents.insert(std::make_pair("atomicMin", Identifier("int atomicMin(inout int mem, int data)\nuint atomicMin(inout uint mem, uint data)\nPerform an atomic min operation to a variable ")));
	idents.insert(std::make_pair("atomicOr", Identifier("int atomicOr(inout int mem, int data)\nuint atomicOr(inout uint mem, uint data)\nPerform an atomic logical OR operation to a variable")));
	idents.insert(std::make_pair("atomicXor", Identifier("int atomicXor(inout int mem, int data)\nuint atomicXor(inout uint mem, uint data)\nPerform an atomic logical exclusive OR operation to a variable")));
}

const ImTextEdit::LanguageDefinition& ImTextEdit::LanguageDefinition::SPIRV()
{
	static bool inited = false;
	static LanguageDefinition langDef;
	if (!inited) {
		/*
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[ \\t]*#[ \\t]*[a-zA-Z_]+", PaletteIndex::Preprocessor));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("\\'\\\\?[^\\']\\'", PaletteIndex::CharLiteral));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/\\;\\,\\.]", PaletteIndex::Punctuation));
		*/

		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::String));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[ =\\t]Op[a-zA-Z]*", PaletteIndex::Keyword));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("%[_a-zA-Z0-9]*", PaletteIndex::Identifier));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::Number));
		
		langDef.mCommentStart = "/*";
		langDef.mCommentEnd = "*/";
		langDef.mSingleLineComment = ";";

		langDef.mCaseSensitive = true;
		langDef.mAutoIndentation = false;

		langDef.mName = "SPIR-V";

		inited = true;
	}
	return langDef;
}

const ImTextEdit::LanguageDefinition& ImTextEdit::LanguageDefinition::C()
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

const ImTextEdit::LanguageDefinition& ImTextEdit::LanguageDefinition::SQL()
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

const ImTextEdit::LanguageDefinition& ImTextEdit::LanguageDefinition::AngelScript()
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

const ImTextEdit::LanguageDefinition& ImTextEdit::LanguageDefinition::Lua()
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