#include <Siv3D.hpp>

# define PrintDebug(x) Print << Unicode::Widen(#x) << U": " << x;
# define ConsoleDebug(x) Console << Unicode::Widen(#x) << U": " << x;

void UpdateText(String& text, int32& cursorPos, int32& selectEndPos, const TextInputMode mode)
{
	String chars = TextInput::GetRawInput();

	auto eraseSelection = [&]() {
		int32 minIndex = Min(cursorPos, selectEndPos);
		int32 maxIndex = Max(cursorPos, selectEndPos);
		//text.erase(minIndex, maxIndex - minIndex);
		cursorPos = minIndex;
		selectEndPos = minIndex;
		};

	if (not chars)
	{
		if (cursorPos != selectEndPos and TextInput::GetEditingText()) {
			eraseSelection();
		}

		return;
	}

	const bool allowEnter = !!(mode & TextInputMode::AllowEnter);
	const bool allowTab = !!(mode & TextInputMode::AllowTab);
	const bool allowBackSpace = !!(mode & TextInputMode::AllowBackSpace);
	const bool allowDelete = !!(mode & TextInputMode::AllowDelete);

	for (auto const ch : chars)
	{
		if (ch == U'\r')
		{
			if (cursorPos != selectEndPos) eraseSelection();

			if (allowEnter)
			{
				text.insert(text.begin() + cursorPos, U'\n');
				++cursorPos;
			}
		}
		else if (ch == U'\b')
		{
			if (cursorPos != selectEndPos) {
				eraseSelection();
				continue;
			}

			if (allowBackSpace && 0 < cursorPos)
			{
				text.erase(text.begin() + cursorPos - 1);
				--cursorPos;
				selectEndPos = cursorPos;
			}
		}
		else if (ch == U'\t')
		{
			if (cursorPos != selectEndPos) eraseSelection();

			if (allowTab)
			{
				text.insert(text.begin() + cursorPos, U'\t');
				++cursorPos;
				selectEndPos = cursorPos;
			}
		}
		else if (ch == char32(0x7F))
		{
			if (cursorPos != selectEndPos) {
				eraseSelection();
				continue;
			}

			if (allowDelete && cursorPos < text.size())
			{
				text.erase(text.begin() + cursorPos);
			}
		}
		else if (not IsControl(ch))
		{
			if (cursorPos != selectEndPos) eraseSelection();

			text.insert(text.begin() + cursorPos, ch);
			++cursorPos;
			selectEndPos = cursorPos;
		}
	}
}


struct OneLineEditableText {
	String text;
	Point topLeft{};
	int32 width{};
	Font font;
	double fontSize{};
	double offsetX = 0.0;
	int32 caretIndex = 0;
	
	int32 selectionEndIndex = 0;
	double leftKeyTimeAccum = 0.0;
	double rightKeyTimeAccum = 0.0;
	bool isFocused = false;

	double editingStartX = 0;
	double editingEndX = 0;
	double caretPosX = 0.0; //| phraseUnderlineStartX | selectionStartX
	double phraseUnderlineEndX = 0.0;
	double selectionEndX = 0.0;
	bool drawPhraseUnderline = true;

	Array<Glyph> editingGlyphs;
	Array<Glyph> glyphs;

	String editingText;

	double timeAfterCaretStopped = 0.0;

	int32 lastCaretIndex = -1;

	String previousText;

public:
	OneLineEditableText() = default;

	OneLineEditableText(StringView _text, const Point _topLeft, int32 _width, const Font& _font, double _fontSize)
		: text(_text)
		, topLeft(_topLeft)
		, width(_width)
		, font(_font)
		, fontSize(_fontSize)
	{
		glyphs = font.getGlyphs(text);
	}

	OneLineEditableText(StringView _text, const Point _topLeft, int32 _width, const Font& _font)
		: text(_text)
		, topLeft(_topLeft)
		, width(_width)
		, font(_font)
		, fontSize(font.fontSize())
	{
		glyphs = font.getGlyphs(text);
	}

	bool hasTextOrEditingText() const
	{
		return text || editingText;
	}

	void update(bool cursorIntersectsArea, double deltaTime = Scene::DeltaTime())
	{
		const double scale = fontSize / font.fontSize();

		int32 height = static_cast<int32>(Ceil(font.height() * scale));

		int32 prevCaretIndex = caretIndex;

		String prevEditingText = editingText;
		editingText = isFocused ? TextInput::GetEditingText() : String();

		if (prevEditingText != editingText) {
			editingGlyphs = font.getGlyphs(editingText);
		}

		if (cursorIntersectsArea) {
			Cursor::RequestStyle(CursorStyle::IBeam);
		}

		if (MouseL.down() and not editingText)
		{
			if (cursorIntersectsArea)
			{
				isFocused = true;
			}
			else
			{
				isFocused = false;
			}
		}

		if (auto rawInput = TextInput::GetRawInput())
		{
			if (rawInput.contains(U'\t') or rawInput.contains(U'\r'))
			{
				isFocused = false;
			}
		}




# if SIV3D_PLATFORM(WEB)
		/*static String prevSyncedText;
		static int32 prevSyncedCaretIndex = -1;*/

		Platform::Web::TextInput::SetFocusToTextInput(isFocused);

		if (isFocused && not editingText)
		{
			if (lastCaretIndex != caretIndex)
			{
				/*Platform::Web::TextInput::SyncronizeText(text);
				Platform::Web::TextInput::SetCursorIndex(caretIndex);

				prevSyncedText = text;
				prevSyncedCaretIndex = caretIndex;*/

			}
			else if (auto currentCursorPos = Platform::Web::TextInput::GetCursorIndex(); lastCaretIndex != currentCursorPos)
			{
				// caretIndex = currentCursorPos;
			}

			// lastCaretIndex = caretIndex;

		}

		/*PrintDebug(prevSyncedText);
		PrintDebug(prevSyncedCaretIndex);*/

		PrintDebug(Platform::Web::TextInput::GetCandicateCursorIndex());

# endif

		bool keyControlOrCommandPressed =
# if SIV3D_PLATFORM(MACOS)
			KeyControl.pressed() || KeyCommand.pressed();
# else
			KeyControl.pressed();
# endif

		// コピー・カット・ペースト処理
		if (not editingGlyphs and isFocused) {

			if (keyControlOrCommandPressed)

				if (keyControlOrCommandPressed and KeyV.down())
				{
					if (caretIndex != selectionEndIndex) {
						int32 minIndex = Min(caretIndex, selectionEndIndex);
						int32 maxIndex = Max(caretIndex, selectionEndIndex);

						Clipboard::SetText(text.substr(minIndex, maxIndex - minIndex));
					}
				}

			if (keyControlOrCommandPressed and KeyX.down())
			{
				if (caretIndex != selectionEndIndex) {
					int32 minIndex = Min(caretIndex, selectionEndIndex);
					int32 maxIndex = Max(caretIndex, selectionEndIndex);

					Clipboard::SetText(text.substr(minIndex, maxIndex - minIndex));
					text.erase(minIndex, maxIndex - minIndex);
					caretIndex = minIndex;
					selectionEndIndex = caretIndex;
				}
			}

			if (keyControlOrCommandPressed and KeyV.down())
			{
				String clipText;
				if (Clipboard::GetText(clipText))
				{
					if (caretIndex != selectionEndIndex)
					{
						int32 minIndex = Min(caretIndex, selectionEndIndex);
						int32 maxIndex = Max(caretIndex, selectionEndIndex);
						text.erase(minIndex, Abs(maxIndex - minIndex));
						caretIndex = minIndex;
						selectionEndIndex = caretIndex;
					}

					clipText.remove_if([](char32 ch) { return IsControl(ch); });
					text.insert(caretIndex, clipText);
					caretIndex += clipText.size();
					selectionEndIndex = caretIndex;
				}
			}
		}

		if (isFocused) {
			UpdateText(text, caretIndex, selectionEndIndex, TextInputMode::AllowBackSpaceDelete);
		}

		bool textChanged = (previousText != text);

		if (textChanged) {
			glyphs = font.getGlyphs(text);
		}

		// キーによるキャレット移動・選択処理
		if (not editingGlyphs and isFocused) {

			if (KeyShift.pressed()) {
				selectionEndIndex += (KeyRight.down() - KeyLeft.down());
			}
			else if (caretIndex == selectionEndIndex) {
				caretIndex += (KeyRight.down() - KeyLeft.down());
				selectionEndIndex = caretIndex;
			}
			else {
				if (KeyLeft.down()) {
					int32 left = Min(caretIndex, selectionEndIndex);
					caretIndex = left;
					selectionEndIndex = left;
				}
				else if (KeyRight.down()) {
					int32 right = Max(caretIndex, selectionEndIndex);
					caretIndex = right;
					selectionEndIndex = right;
				}
			}

			if (KeyRight.pressed() and not KeyLeft.pressed())
			{
				rightKeyTimeAccum += deltaTime;
				if (rightKeyTimeAccum > 0.5)
				{
					if (KeyShift.pressed()) {
						selectionEndIndex++;
					}
					else if (caretIndex == selectionEndIndex) {
						caretIndex++;
						selectionEndIndex = caretIndex;
					}
					else {
						int32 right = Max(caretIndex, selectionEndIndex);
						caretIndex = right;
						selectionEndIndex = right;
					}
					rightKeyTimeAccum -= 0.03125;
				}
			}
			else
			{
				rightKeyTimeAccum = 0.0;
			}

			if (KeyLeft.pressed() and not KeyRight.pressed())
			{
				leftKeyTimeAccum += deltaTime;
				if (leftKeyTimeAccum > 0.5)
				{
					if (KeyShift.pressed()) {
						selectionEndIndex--;
					}
					else if (caretIndex == selectionEndIndex) {
						caretIndex--;
						selectionEndIndex = caretIndex;
					}
					else {
						int32 left = Min(caretIndex, selectionEndIndex);
						caretIndex = left;
						selectionEndIndex = left;
					}
					leftKeyTimeAccum -= 0.03125;
				}
			}
			else
			{
				leftKeyTimeAccum = 0.0;
			}


			caretIndex = Clamp<int32>(caretIndex, 0, glyphs.size());
			selectionEndIndex = Clamp<int32>(selectionEndIndex, 0, glyphs.size());


			if (KeyHome.down())
			{
				if (KeyShift.pressed()) {
					caretIndex = 0;
				}
				else {
					caretIndex = 0;
					selectionEndIndex = caretIndex;
				}
			}

			if (KeyEnd.down())
			{
				if (KeyShift.pressed()) {
					caretIndex = static_cast<int32>(glyphs.size());
				}
				else {
					caretIndex = static_cast<int32>(glyphs.size());
					selectionEndIndex = caretIndex;
				}
			}

			if (keyControlOrCommandPressed and KeyA.down())
			{
				caretIndex = static_cast<int32>(glyphs.size());
				selectionEndIndex = 0;
			}

		}

		// マウスクリックによるキャレット移動・選択処理
		if (not editingGlyphs and isFocused) {

			if (MouseL.down())
			{
				Vec2 pos = topLeft + Vec2{ offsetX, 0 };
				for (auto [i, glyph] : Indexed(glyphs)) {
					double xAdvance = glyph.xAdvance * scale;
					if (Cursor::Pos().x < pos.x + xAdvance / 2)
					{
						selectionEndIndex = i;
						break;
					}
					pos.x += xAdvance;
					if (i + 1 == glyphs.size())
					{
						selectionEndIndex = glyphs.size();
					}
				}
			}

			if (MouseL.pressed())
			{
				Vec2 pos = topLeft + Vec2{ offsetX, 0 };
				for (auto [i, glyph] : Indexed(glyphs)) {
					double xAdvance = glyph.xAdvance * scale;
					if (Cursor::Pos().x < pos.x + xAdvance / 2)
					{
						caretIndex = i;
						break;
					}
					pos.x += xAdvance;
					if (i + 1 == glyphs.size())
					{
						caretIndex = glyphs.size();
					}
				}
			}

		}

#if SIV3D_PLATFORM(WEB)
		static String prevSyncedText;
		static int32 prevSyncedCaretIndex = -1;


		if (not editingText) {

			if (textChanged)
			{
				String text_copy = text;
				Platform::Web::TextInput::SyncronizeText(text_copy);
				prevSyncedText = text_copy;
			}

			if (prevCaretIndex != caretIndex)
			{
				Platform::Web::TextInput::SetCursorIndex(caretIndex);
				prevSyncedCaretIndex = caretIndex;
			}
		}
		

		PrintDebug(prevSyncedText);
		PrintDebug(prevSyncedCaretIndex);
#endif

		PrintDebug(editingText);

		for (auto c : editingText) {
			PrintDebug(U"{} : {}"_fmt(c, ToHex(c)));
		}
		PrintDebug(editingGlyphs.size());
		PrintDebug(caretIndex);


		int32 caretIndexInIME = 0;
		int32 imeTextCursorLength = 0;
#if SIV3D_PLATFORM(WINDOWS)
		std::tie(caretIndexInIME, imeTextCursorLength) = Platform::Windows::TextInput::GetCursorIndex();
#elif SIV3D_PLATFORM(WEB)
		if (editingText) {
			caretIndexInIME = Platform::Web::TextInput::GetCursorIndex() - caretIndex;
		}
#endif

		PrintDebug(caretIndexInIME);
		PrintDebug(imeTextCursorLength);

		PrintDebug(Platform::Web::TextInput::GetCursorIndex());

		drawPhraseUnderline = imeTextCursorLength > 0;


		// キャレット・選択範囲の描画位置計算
		double allTextLastX = 0.0;
		{
			double posX = topLeft.x + offsetX;

			if (caretIndex == 0)
			{
				editingStartX = posX;
				caretPosX = posX;
				for (const auto [i, editingGlyph] : Indexed(editingGlyphs))
				{

					posX += editingGlyph.xAdvance * scale;

					if (i + 1 == caretIndexInIME)
					{
						caretPosX = posX;
					}

					if (i + 1 == caretIndexInIME + imeTextCursorLength)
					{
						phraseUnderlineEndX = posX;
					}
				}
				editingEndX = posX;
			}

			if (selectionEndIndex == 0)
			{
				selectionEndX = posX;
			}

			for (auto [i, glyph] : Indexed(glyphs)) {
				posX += glyph.xAdvance * scale;

				if (i + 1 == caretIndex)
				{
					editingStartX = posX;
					caretPosX = posX;
					for (const auto [i, editingGlyph] : Indexed(editingGlyphs))
					{

						posX += editingGlyph.xAdvance * scale;

						if (i + 1 == caretIndexInIME)
						{
							caretPosX = posX;
						}

						if (i + 1 == caretIndexInIME + imeTextCursorLength)
						{
							phraseUnderlineEndX = posX;
						}
					}
					editingEndX = posX;
				}

				if (i + 1 == selectionEndIndex)
				{
					selectionEndX = posX;
				}
			}
			allTextLastX = posX;
		}

		double previousOffsetX = offsetX;

		if (caretPosX > topLeft.x + width)
		{
			offsetX -= (caretPosX - (topLeft.x + width));
		}
		else if (caretPosX < topLeft.x)
		{
			offsetX += (topLeft.x - caretPosX);
		}

		if (allTextLastX < topLeft.x + width)
		{
			offsetX += (topLeft.x + width) - (allTextLastX);
		}

		if (offsetX > 0.0)
		{
			offsetX = 0.0;
		}

		double offsetDiff = (offsetX - previousOffsetX);

		caretPosX += offsetDiff;
		selectionEndX += offsetDiff;
		editingStartX += offsetDiff;
		editingEndX += offsetDiff;
		phraseUnderlineEndX += offsetDiff;



		previousText = text;
	}

	void draw(const ColorF& textColor = Palette::Black, const ColorF& selectingAreaColor = ColorF(0.0, 0.5, 1.0, 0.2)) const {
		const double scale = fontSize / font.fontSize();

		int32 height = static_cast<int32>(Ceil(font.height() * scale));

		Graphics2D::SetScissorRect(Rect{ topLeft, width + 1, height + 1 });
		RasterizerState rs = RasterizerState::Default2D;
		rs.scissorEnable = true;
		{
			ScopedRenderStates2D scissor{ rs };

			if (isFocused) {
				// 編集中文字列の下線
				if (editingGlyphs) {
					Rect{ static_cast<int32>(editingStartX), topLeft.y + height, static_cast<int32>(editingEndX) - static_cast<int32>(editingStartX), 1 }.draw(textColor);
				}

				if (caretIndex != selectionEndIndex)
				{
					// 選択範囲
					double minX = Min(caretPosX, selectionEndX);
					double maxX = Max(caretPosX, selectionEndX);
					Rect{ static_cast<int32>(minX), topLeft.y, static_cast<int32>(maxX) - static_cast<int32>(minX), height }.draw(selectingAreaColor);
				}
				else {
					if (drawPhraseUnderline)
					{
						// 編集中文節の下線
						Rect{ static_cast<int32>(caretPosX), topLeft.y + height - 2, static_cast<int32>(phraseUnderlineEndX) - static_cast<int32>(caretPosX), 2 }.draw(textColor);
					}
					else {
						// キャレット
						Rect{ static_cast<int32>(caretPosX), static_cast<int32>(topLeft.y), 1, height }.draw(textColor);
					}
				}
			}

			// 文字列本体の描画
			{
				ScopedCustomShader2D shader{ Font::GetPixelShader(font.method()) };

				Vec2 pos = topLeft + Vec2{ offsetX, 0 };


				auto drawEditingGlyphs = [&]()
					{
						for (const auto& editingGlyph : editingGlyphs)
						{
							editingGlyph.texture.scaled(scale).draw(pos + editingGlyph.getOffset() * scale, textColor);
							pos.x += editingGlyph.xAdvance * scale;
						}
					};

				if (caretIndex == 0 and editingGlyphs)
				{
					drawEditingGlyphs();
				}

				for (auto [i, glyph] : Indexed(glyphs)) {
					glyph.texture.scaled(scale).draw(pos + glyph.getOffset() * scale, textColor);
					pos.x += glyph.xAdvance * scale;

					if (i + 1 == caretIndex and editingGlyphs)
					{
						drawEditingGlyphs();
					}
				}

			}
		}
	}
};

struct TextBox {
	RoundRect body;
	OneLineEditableText editableText;
	double mouseOverTransition = 0.0;
	double mouseOverTransitionVelocity = 0.0;

	TextBox(Vec2 position, double width) {
		body = RoundRect{ position.x, position.y, width, 40, 20 };
		editableText = OneLineEditableText{ U"Hello, Siv3D!", position.asPoint() + Point{16, 5} , static_cast<int32>(width) - 32, SimpleGUI::GetFont() };
	};

	void update() {
		bool mouseOver = body.intersects(Cursor::Pos());
		editableText.update(mouseOver);
		mouseOverTransition = Math::SmoothDamp(mouseOverTransition, mouseOver ? 1.0 : 0.0, mouseOverTransitionVelocity, 0.05);
	}

	void draw() {
		body.draw(Palette::White);
		if (editableText.isFocused) {
			body.drawFrame(0.0, 1.5, ColorF{ 0.35, 0.7, 1.0, 0.75 })
				.drawFrame(2.5, 0.0, ColorF{ 0.35, 0.7, 1.0 });

			//body.drawFrame(2, ColorF{ 0.35, 0.7, 1.0 });
		}
		else {
			body.draw(ColorF(0.9, mouseOverTransition));
			body.drawFrame(2, Palette::Gray);
		}
		editableText.draw();
		if (not editableText.hasTextOrEditingText())
		{
			editableText.font(U" Enter text...").draw(editableText.fontSize, editableText.topLeft, Palette::Gray);
		}
	}
};

void Main()
{

	//https://github.com/Raclamusi/OpenSiv3D/issues/7 BackSpaceが効かない問題の回避策
#if SIV3D_PLATFORM(WEB) 
	EM_ASM({
		let keyDownEvent = null;
		let timeoutId = null;

		addEventListener("keydown", function(event) {
			if (!event.isTrusted) {
				return;
			}
			keyDownEvent = event;
		});

		addEventListener("keyup", function(event) {
			if (!event.isTrusted) {
				return;
			}
			const keyUpEvent = event;
			if (keyDownEvent.timeStamp == keyUpEvent.timeStamp) {
				clearTimeout(timeoutId);
				dispatchEvent(keyDownEvent);
				timeoutId = setTimeout(function() {
					dispatchEvent(keyUpEvent);
					timeoutId = null;
				}, 100);
			}
		});
		});
#endif

	// Scene::SetBackground(ColorF(1));

	//Window::SetStyle(WindowStyle::Sizable);
	//Window::Maximize();
	//Scene::SetResizeMode(ResizeMode::Actual);
	//Window::SetFullscreen(true);

	//MSゴシック
	const FilePath path = (FileSystem::GetFolderPath(SpecialFolder::SystemFonts) + U"msgothic.ttc");
	//Font font{ FontMethod::MSDF,48 ,path};
	Font font = SimpleGUI::GetFont();

	// ConsoleDebug(font.fontSize());

	TextEditState textEditState{ U"Hello, Siv3D!" };

	TextBox editableTextBox{ Vec2{ 100, 100 }, 250 };

	double time1 = 0.0;
	double time2 = 0.0;

	while (System::Update())
	{
		ClearPrint();

		Stopwatch timer{ StartImmediately::Yes };
		editableTextBox.update();

		editableTextBox.draw();
		Graphics2D::Flush();

		constexpr double k = 0.01;
		time1 = k * timer.us() + (1.0 - k) * time1;

		PrintDebug(time1);

		Stopwatch timer2{ StartImmediately::Yes };

		SimpleGUI::TextBox(textEditState, Vec2{ 100, 200 }, 400);
		Graphics2D::Flush();
		time2 = k * timer2.us() + (1.0 - k) * time2;
		PrintDebug(time2);

		// Platform::Windows::TextInput::DrawCandidateWindow(font, Vec2{ editableTextBox.editableText.editingStartX, editableTextBox.body.rect.bottomY() });

	}

}
