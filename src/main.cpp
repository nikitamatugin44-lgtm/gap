#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <string>

using namespace geode::prelude;

static std::string num(double v) {
	return fmt::format("{:.9g}", v);
}

// the window opened from the button in the Edit tab
class MicroPopup : public Popup {
protected:
	TextInput* m_dx = nullptr;
	TextInput* m_dy = nullptr;
	CCLabelBMFont* m_info = nullptr;
	CCLabelBMFont* m_note = nullptr;

	CCArray* selection() {
		auto ui = EditorUI::get();
		if (!ui) return nullptr;
		return ui->getSelectedObjects();
	}

	// shows the position of the first selected object and the smallest step there
	void refresh() {
		auto arr = this->selection();
		if (!arr || arr->count() == 0) {
			m_info->setString("No object selected");
			return;
		}

		auto first = static_cast<GameObject*>(arr->objectAtIndex(0));
		float x = first->getPositionX();
		float y = first->getPositionY();
		float inf = std::numeric_limits<float>::infinity();
		float sx = std::nextafter(x, inf) - x;
		float sy = std::nextafter(y, inf) - y;

		std::string text = fmt::format(
			"{} object(s)\nX = {}   Y = {}\nsmallest step here: X {:.3g}   Y {:.3g}",
			static_cast<int>(arr->count()), num(x), num(y), sx, sy
		);
		m_info->setString(text.c_str());
	}

	bool init() {
		if (!Popup::init(360.f, 250.f)) return false;

		this->setTitle("Micro Move");

		m_info = CCLabelBMFont::create("", "bigFont.fnt");
		m_info->setScale(0.32f);
		m_mainLayer->addChildAtPosition(m_info, Anchor::Center, ccp(0.f, 68.f));

		m_dx = TextInput::create(120.f, "dX", "bigFont.fnt");
		m_dx->setFilter("0123456789.-+eE");
		m_mainLayer->addChildAtPosition(m_dx, Anchor::Center, ccp(-70.f, 20.f));

		m_dy = TextInput::create(120.f, "dY", "bigFont.fnt");
		m_dy->setFilter("0123456789.-+eE");
		m_mainLayer->addChildAtPosition(m_dy, Anchor::Center, ccp(70.f, 20.f));

		auto moveSpr = ButtonSprite::create("Move by dX / dY");
		moveSpr->setScale(0.7f);
		auto moveBtn = CCMenuItemSpriteExtra::create(
			moveSpr, this, menu_selector(MicroPopup::onMove)
		);
		m_buttonMenu->addChildAtPosition(moveBtn, Anchor::Center, ccp(0.f, -15.f));

		const char* names[4] = { "X +1 step", "X -1 step", "Y +1 step", "Y -1 step" };
		const float xs[4] = { -125.f, -42.f, 42.f, 125.f };
		for (int i = 0; i < 4; i++) {
			auto spr = ButtonSprite::create(names[i]);
			spr->setScale(0.5f);
			auto btn = CCMenuItemSpriteExtra::create(
				spr, this, menu_selector(MicroPopup::onStep)
			);
			btn->setTag(i + 1);
			m_buttonMenu->addChildAtPosition(btn, Anchor::Center, ccp(xs[i], -55.f));
		}

		m_note = CCLabelBMFont::create("", "bigFont.fnt");
		m_note->setScale(0.3f);
		m_mainLayer->addChildAtPosition(m_note, Anchor::Center, ccp(0.f, -92.f));

		auto hint = CCLabelBMFont::create(
			"Positions are stored as 32-bit floats: the smaller the move than the step, the less it changes.",
			"bigFont.fnt"
		);
		hint->setScale(0.22f);
		m_mainLayer->addChildAtPosition(hint, Anchor::Center, ccp(0.f, -112.f));

		this->refresh();
		return true;
	}

	// move by the typed offset
	void onMove(CCObject*) {
		auto ui = EditorUI::get();
		auto arr = this->selection();
		if (!ui || !arr || arr->count() == 0) {
			m_note->setString("Select an object first");
			return;
		}

		float dx = static_cast<float>(std::strtod(m_dx->getString().c_str(), nullptr));
		float dy = static_cast<float>(std::strtod(m_dy->getString().c_str(), nullptr));

		auto first = static_cast<GameObject*>(arr->objectAtIndex(0));
		float bx = first->getPositionX();
		float by = first->getPositionY();

		for (auto obj : CCArrayExt<GameObject*>(arr)) {
			ui->moveObject(obj, CCPoint(dx, dy));
		}

		bool same = first->getPositionX() == bx && first->getPositionY() == by;
		if (same && (dx != 0.f || dy != 0.f)) {
			m_note->setString("Nothing changed: the move is below the smallest step here.");
		} else {
			m_note->setString("Moved.");
		}
		this->refresh();
	}

	// move by exactly one smallest step the game can store
	void onStep(CCObject* sender) {
		auto ui = EditorUI::get();
		auto arr = this->selection();
		if (!ui || !arr || arr->count() == 0) {
			m_note->setString("Select an object first");
			return;
		}

		int tag = static_cast<CCNode*>(sender)->getTag();
		float inf = std::numeric_limits<float>::infinity();

		for (auto obj : CCArrayExt<GameObject*>(arr)) {
			float x = obj->getPositionX();
			float y = obj->getPositionY();
			CCPoint off(0.f, 0.f);

			if (tag == 1) off.x = std::nextafter(x, inf) - x;
			if (tag == 2) off.x = std::nextafter(x, -inf) - x;
			if (tag == 3) off.y = std::nextafter(y, inf) - y;
			if (tag == 4) off.y = std::nextafter(y, -inf) - y;

			ui->moveObject(obj, off);
		}

		m_note->setString("Moved by one step.");
		this->refresh();
	}

public:
	static MicroPopup* create() {
		auto ret = new MicroPopup();
		if (ret->init()) {
			ret->autorelease();
			return ret;
		}
		delete ret;
		return nullptr;
	}
};

class $modify(MicroEditorUI, EditorUI) {
public:
	bool init(LevelEditorLayer* editorLayer) {
		if (!EditorUI::init(editorLayer)) return false;

		if (!m_editButtonBar || !m_editButtonBar->m_buttonArray) return true;

		// a green button with a magnifier on it
		auto holder = CCNode::create();
		holder->setContentSize({ 40.f, 40.f });

		if (auto bg = CCSprite::createWithSpriteFrameName("GJ_button_01.png")) {
			float h = bg->getContentSize().height;
			if (h > 1.f) bg->setScale(40.f / h);
			bg->setPosition({ 20.f, 20.f });
			holder->addChild(bg);
		}
		if (auto icon = CCSprite::createWithSpriteFrameName("GJ_zoomInBtn_001.png")) {
			icon->setScale(0.45f);
			icon->setPosition({ 20.f, 20.f });
			holder->addChild(icon);
		}

		auto btn = CCMenuItemSpriteExtra::create(
			holder, this, menu_selector(MicroEditorUI::onMicro)
		);
		btn->setID("micro-move-button"_spr);
		m_editButtonBar->m_buttonArray->addObject(btn);

		// rebuild the bar so the new button shows up
		int a = GameManager::get()->getIntGameVariable("0049");
		int b = GameManager::get()->getIntGameVariable("0050");
		int rows = 2;
		int cols = 6;
		if (a > 0 && b > 0) {
			rows = std::min(a, b);
			cols = std::max(a, b);
		}
		m_editButtonBar->reloadItems(rows, cols);

		return true;
	}

	void onMicro(CCObject*) {
		if (auto p = MicroPopup::create()) p->show();
	}
};
