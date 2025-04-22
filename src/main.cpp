#include "GUI/CCControlExtension/CCScale9Sprite.h"
#include "Geode/cocos/CCDirector.h"
#include "Geode/cocos/actions/CCActionEase.h"
#include "Geode/cocos/actions/CCActionInstant.h"
#include "Geode/cocos/actions/CCActionInterval.h"
#include "Geode/cocos/cocoa/CCObject.h"
#include "Geode/cocos/label_nodes/CCLabelBMFont.h"
#include "Geode/cocos/layers_scenes_transitions_nodes/CCLayer.h"
#include "Geode/cocos/menu_nodes/CCMenuItem.h"
#include "Geode/cocos/platform/CCPlatformMacros.h"
#include "Geode/ui/BasedButtonSprite.hpp"
#include "Geode/ui/Layout.hpp"
#include "Geode/ui/Popup.hpp"
#include "Geode/ui/TextInput.hpp"
#include "Geode/utils/string.hpp"
#include "ccTypes.h"
#include <Geode/Geode.hpp>
#include <Geode/binding/CreatorLayer.hpp>
#include <Geode/modify/CreatorLayer.hpp>
#include <Geode/modify/CCBlockLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/EndLevelLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <nlohmann/json.hpp>

class LoadingCircleLayer : public cocos2d::CCLayer {
public:
    cocos2d::CCSprite *m_pCircle;

    bool init() {
        m_pCircle = cocos2d::CCSprite::create("loadingCircle.png");
        this->addChild(m_pCircle);

        m_pCircle->setBlendFunc({GL_SRC_ALPHA, GL_ONE});

        scheduleUpdate();

        return true;
    }
    CREATE_FUNC(LoadingCircleLayer);

    void update(float delta) {
        if(m_pCircle) {
            float rot = m_pCircle->getRotation();
            rot += 500 * delta;
            if (rot > 360) {
                rot -= 360;
            }
            m_pCircle->setRotation(rot);
        }
    }
};

using namespace geode::prelude;

#include <functional>
#include <Geode/utils/web.hpp>

class GeodeNetwork {
public:
    enum HttpMethod {
        MGet
    };
protected:
    std::function<void(GeodeNetwork *)> _onOk = nullptr;
    std::function<void(GeodeNetwork *)> _onError = nullptr;

    std::string _data;
    std::string _url;

    HttpMethod _method = MGet;

    geode::EventListener<geode::utils::web::WebTask> _listener;

    void setupListener();
public:
    GeodeNetwork();

    void setOkCallback(std::function<void(GeodeNetwork *)> ok);
    void setErrorCallback(std::function<void(GeodeNetwork *)> error);

    void setURL(std::string url);
    void setMethod(HttpMethod method);

    std::string &getResponse();

    void unsetCallbacks() {
        _onOk = nullptr;
        _onError = nullptr;
    }

    void send();
};

void GeodeNetwork::setOkCallback(std::function<void(GeodeNetwork *)> ok) {
    _onOk = ok;
}
void GeodeNetwork::setErrorCallback(std::function<void(GeodeNetwork *)> error) {
    _onError = error;
}

void GeodeNetwork::setURL(std::string url) {
    _url = url;
}
void GeodeNetwork::setMethod(HttpMethod method) {
    _method = method;
}

std::string &GeodeNetwork::getResponse() {
    return _data;
}

void GeodeNetwork::send() {
    geode::utils::web::WebRequest req = geode::utils::web::WebRequest();

    req.timeout(std::chrono::seconds(10));

    geode::utils::web::WebTask task;

    if (_method == MGet) {
        task = req.get(_url);
    }

    _listener.setFilter(task);
}

GeodeNetwork::GeodeNetwork() {
    setupListener();
}

void GeodeNetwork::setupListener() {
    _listener.bind([this] (geode::utils::web::WebTask::Event* e) {
        if (geode::utils::web::WebResponse* res = e->getValue()) {
            this->_data = res->string().unwrapOr("Not a string");

            // log::info("_data = {}", this->_data);

            if (res->ok() && this->_onOk != nullptr) {
                this->_onOk(this);

                return;
            }

            if (!res->ok() && this->_onError != nullptr) {
                this->_onError(this);

                return;
            }
        } else if (e->isCancelled()) {
            this->_data = "Error: cancelled";

            if (this->_onError != nullptr) {
                this->_onError(this);

                return;
            }
        }
    });
}

class LevelProgressionState {
public:
    enum Difficulty {
        SDNa = -1,
        SDDemons = -2,
        SDEasy = 1,
        SDNormal = 2,
        SDHard = 3,
        SDHarder = 4,
        SDInsane = 5
    };
    enum DemonDiff {
        SDDNone = 0,
        SDDEasy = 1,
        SDDMedium = 2,
        SDDHard = 3,
        SDDInsane = 4,
        SDDExtreme = 5
    };

    Difficulty std_dif = SDEasy;
    DemonDiff demon_dif = SDDNone;
    int lives = -255;
    int lpd = 0;
    int cur_l = 0;

    GeodeNetwork *net = nullptr;

    LevelCell *cell = nullptr;
    GJGameLevel *level = nullptr;

    bool in_playlayer = false;

    bool began = false;

    std::string seed = "";
    bool seedSet = false;

    bool clickedBegin = false;
    bool beganGameover = false;

    void initNet() {
        if (net) return;
        net = new GeodeNetwork();
    }
    void init() {
        if (lives <= -255) {
            lives = Mod::get()->getSettingValue<int>("lives");
            log::info("{} lives avaiable", lives);
        }
        lpd = Mod::get()->getSettingValue<int>("levels-per-diff");
        log::info("{} levels per difficulty", lpd);
        initNet();
    }
};

class LevelProgressionLives;
class LevelProgressionPopup;

namespace LPGlobal {
    struct LevelProgressionState state = {};
    LevelProgressionLives *overlay = nullptr;
    LevelProgressionPopup *curPopup = nullptr;

    bool applyBounceAnimate(cocos2d::CCNode *node, float power) {
        if (node == nullptr /*|| node->getActionByTag(0x1000) != nullptr || node->getActionByTag(0x2000) != nullptr*/ || power <= 0.f) {
            return false;
        }

        auto kf1 = CCEaseElasticOut::create(
            CCMoveBy::create(0.5f, {0.f, 10.f * power}),
            2.f
        );
        kf1->setTag(0x1000);

        auto kf2 = CCSequence::createWithTwoActions(
            CCDelayTime::create(0.1f),
            CCEaseBounceOut::create(
                CCMoveBy::create(0.5f, {0.f, -10.f * power})
            )
        );
        kf2->setTag(0x2000);

        node->runAction(kf1);
        node->runAction(kf2);

        return true;
    }
}

#define PARSE_STRING(lvalue, rvalue) if (!rvalue.is_null()) lvalue = rvalue.get<std::string>().c_str()
#define PARSE_INT(lvalue, rvalue) if (!rvalue.is_null()) lvalue = rvalue.get<int>()
#define PARSE_BOOL(lvalue, rvalue) if (!rvalue.is_null()) lvalue = (int)(rvalue.get<bool>())

class LevelProgressionLives : public CCNode {
private:
    LevelProgressionState *_state;
    float _shakeLevel = 0.f;

    CCNode *_baseNode;
    CCLabelBMFont *_label;
    SimplePlayer *_icon;
    CCScale9Sprite *_bg;
public:
    LevelProgressionState *getState() {
        log::info("lives: getstate: {}", (void *)_state);
        return _state;
    }

    bool init(LevelProgressionState *state) {
        if (!state || !CCNode::init()) return false;

        setID("lp-overlay");

        state->init();

        LPGlobal::overlay = this;

        CCNode *base = CCNode::create();
        auto gm = GameManager::sharedState();

        SimplePlayer *p = SimplePlayer::create(gm->getPlayerFrame());
        p->setColors(gm->colorForIdx(gm->getPlayerColor()), gm->colorForIdx(gm->getPlayerColor2()));

        p->setID("player-icon");
        p->m_firstLayer->setRotation(15.f);
        p->m_firstLayer->setScale(0.8f);

        std::string lives = "x" + std::to_string(state->lives);

        CCLabelBMFont *f = CCLabelBMFont::create(lives.c_str(), "gjFont59.fnt", 128.f, kCCTextAlignmentLeft);
        f->setOpacity(190);
        f->setScale(0.8f);
        f->setAnchorPoint({0, 0.5});

        auto pSz = p->m_firstLayer->getContentSize();
        auto fSz = f->getContentSize() * f->getScale();

        p->setPosition({pSz.width / 2, pSz.height / 2});
        f->setPosition({p->getPosition().x + pSz.width + 2.f - (pSz.width / 2), fSz.height / 2});

        setContentSize({f->getPosition().x + fSz.width, std::max(pSz.height, fSz.height)});

        base->addChild(p, 1);
        base->addChild(f, 2);

        auto col = CCScale9Sprite::create("square02_small.png");
        col->setContentSize(getContentSize());
        col->setOpacity(80.f);
        col->setAnchorPoint({0,0});
        base->addChild(col, -1);

        base->setContentSize(getContentSize());
        base->setPosition({0,0});

        addChild(base);

        _baseNode = base;
        _label = f;
        _state = state;
        _icon = p;
        _bg = col;

        scheduleUpdate();

        return true;
    }

    static LevelProgressionLives* create(LevelProgressionState *state) {
        auto ret = new LevelProgressionLives();
        if (ret->init(state)) {
            ret->autorelease();
            return ret;
        }

        delete ret;
        return nullptr;
    }

    void shake() {
        _shakeLevel = 1.f;
    }
    void updateLabel() {
        std::string lives = "x" + std::to_string(_state->lives);
        _label->setString(lives.c_str());

        auto fSz = _label->getContentSize() * _label->getScale();
        auto pSz = _icon->m_firstLayer->getContentSize();

        setContentSize({_label->getPosition().x + fSz.width, std::max(pSz.height, fSz.height)});
        _baseNode->setContentSize(getContentSize());
        _bg->setContentSize(getContentSize());
    }
    void showCoinAnimation() {
        CCCircleWave *wave = CCCircleWave::create(1.f, 80.f, 0.55f, true, true);
        CCSprite *coin = CCSprite::createWithSpriteFrameName("secretCoin_01_001.png");
        coin->setScale(0.1f);
        coin->setOpacity(0);
        coin->runAction(
            CCSequence::create(
                CCFadeTo::create(0.15f, 255),
                CCDelayTime::create(0.15f),
                CCFadeTo::create(0.25f, 0),
                nullptr
            )
        );
        int degree1 = (rand() % 360) * (((rand() % 2) == 0) ? 1 : -1);
        int degree2 = (rand() % 360) * (((rand() % 2) == 0) ? 1 : -1);
        coin->setRotation(degree1);
        coin->runAction(CCRotateTo::create(1.5f, degree2));
        coin->runAction(CCEaseSineOut::create(CCScaleTo::create(0.45f, 1.5f)));
        coin->setPosition(getContentSize() / 2.f);
        wave->setPosition(getContentSize() / 2.f);
        wave->m_color = ccYELLOW;
        wave->m_opacity = 0.45f;
        addChild(coin, 2);
        addChild(wave, 1);
    }
    void decreaseLives() {
        _state->lives--;
        updateLabel();
        shake();
        // showCoinAnimation();
    }
    void increaseLives() {
        _state->lives++;
        updateLabel();
        LPGlobal::applyBounceAnimate(this, 1.5f);
        showCoinAnimation();
    }

    void update(float delta) override {
        if (_shakeLevel > 0) {
            float offset = ((float)(rand() % 0xFFFF) / (float)0xFFFF) * 24.f * _shakeLevel;
            if ((rand() % 2) == 0) {
                offset = -offset;
            }
            _baseNode->setPositionX(offset);

            _shakeLevel -= delta * 3.f;
        } else {
            _baseNode->setPositionX(0);
        }
    }
};

class LevelProgressionRandPopup : public geode::Popup<LevelProgressionState*> {
protected:
    geode::TextInput *_seedInput = nullptr;
    LevelProgressionState *_prState = nullptr;
    std::string _inputBefore = "";
public:
    bool setup(LevelProgressionState *state) override {
        setTitle("Set Seed");
        m_mainLayer->setID("main-layer");

        if (state == nullptr) {
            log::info("state is nullptr");
            return true;
        } else {
            log::info("randpopup: state: {}", (void *)state);
        }

        _prState = state;

        _seedInput = TextInput::create(150, "");
        if (_prState->seedSet) {
            _seedInput->setString(_prState->seed);
        }
        _seedInput->setFilter("0123456789");
        _seedInput->setCallback([state](const std::string &i) {
            log::info("setting {} to {}", state->seed, i);
            state->seed = i;
            state->seedSet = true;
            log::info("result: {}", state->seed);
        });
        _seedInput->getInputNode()->setTouchPriority(-1500);
        _seedInput->getInputNode()->m_selected = true;
        setTouchPriority(-1500);

        m_mainLayer->addChildAtPosition(_seedInput, Anchor::Center);

        _inputBefore = _seedInput->getString();

        return true;
    }

    static LevelProgressionRandPopup* create(LevelProgressionState *state) {
        auto ret = new LevelProgressionRandPopup();
        if (ret->initAnchored(190.f, 100.f, state)) {
            ret->autorelease();
            return ret;
        }

        delete ret;
        return nullptr;
    }

    LevelProgressionState *getState() {
        log::info("randpopup: getstate: {}", (void *)_prState);
        return _prState;
    }

    CCSize getRealContentSize() {
        return m_mainLayer->getContentSize();
    }

    void onClose(cocos2d::CCObject*o) override;
};

class LevelProgressionPopup : public geode::Popup<>, public MusicDownloadDelegate {
protected:
    CCMenuItemSpriteExtra *_playBtn;
    LoadingCircleLayer *_circle;
    CCMenu *_buttons = nullptr;
    CustomSongWidget *_widget = nullptr;
    std::string _cachedLevelString = "";
    LevelCell *_currentCell = nullptr;

    std::vector<CCNode*> _extraNodes = {};

    int _filesTotal = 0;
    int _filesDownloaded = 0;
public:
    void onClose(cocos2d::CCObject*o) override {
        if (PlayLayer::get() == nullptr) {
            auto st = getState();
            st->clickedBegin = false;
            st->began = false;
            st->net->unsetCallbacks();
        } else {
            return;
        }
        geode::Popup<>::onClose(o);
    }

    void updateTitle() {
        if (_filesDownloaded == _filesTotal) {
            setTitle("Difficulty Progression");
        } else {
            std::string title = fmt::format("Downloading files ({}/{} done)", _filesDownloaded, _filesTotal);
            setTitle(title);
        }
    }

    void increaseDownloadedStat() {
        _filesDownloaded++;
        updateTitle();
    }

    void downloadSongFinished(int id) override {
        log::info("{} has been downloaded", id);
        increaseDownloadedStat();
    }
    void downloadSongFailed(int id, GJSongError p1) override {
        log::info("{} could not be downloaded: {}", id, (int)p1);
        increaseDownloadedStat();
        setTitle("Song download error");
    }
    void downloadSFXFinished(int id) override {
        log::info("{} (SFX) has been downloaded", id);
        increaseDownloadedStat();
    }
    void downloadSFXFailed(int id, GJSongError p1) override {
        log::info("{} (SFX) could not be downloaded: {}", id, (int)p1);
        increaseDownloadedStat();
        setTitle("SFX download error");
    }

    void waitForMusicInfo(float) {
        // schedule(schedule_selector(LevelProgressionPopup::waitForMusicInfo), 0.5f);

        if (!_widget || !_widget->m_downloadBtn->isVisible()) return;
        log::info("DOWNLOADING CONTENT");
        _widget->m_downloadBtn->activate();
        unschedule(schedule_selector(LevelProgressionPopup::waitForMusicInfo));
    }

    void processOnDownloaed() {
        log::info("processing downloaded level");
        auto glm = GameLevelManager::get();
        auto st = getState();
        auto saved = glm->getSavedLevel(st->level->m_levelID);
        log::info("1={} 2={}", saved->m_levelString, st->level->m_levelString);
        _cachedLevelString = saved->m_levelString;

        saved->m_creatorName = st->level->m_creatorName;
        saved->m_accountID = st->level->m_accountID;
        std::string lstr = st->level->m_levelString;
        st->level = saved;
        if (!lstr.empty() && st->level->m_levelString.empty()) {
            log::info("replacing {} with {}", st->level->m_levelString, lstr);
            st->level->m_levelString = lstr;
        }
        glm->saveLevel(st->level);
        std::vector<int> songs = {};
        std::vector<int> effects = {};

        if (st->level->m_songID != 0) {
            songs.push_back(st->level->m_songID);
        }
        auto vs = geode::utils::string::split(st->level->m_songIDs, ",");
        auto vs2 = geode::utils::string::split(st->level->m_sfxIDs, ",");

        for (std::string &id : vs) {
            songs.push_back(std::stoi(id));
        }
        for (std::string &id : vs2) {
            effects.push_back(std::stoi(id));
        }

        log::info("song_list={} songs_size={} sfx_size={}", st->level->m_songIDs, vs.size(), vs2.size());

        auto musman = MusicDownloadManager::sharedState();
        bool added_as_delegate = false;
        for (int id : songs) {
            if (musman->isSongDownloaded(id)) continue;

            if (!added_as_delegate) {
                musman->addMusicDownloadDelegate(this);
                added_as_delegate = true;
            }

            log::info("downloading {}", id);
            // if (id < 10000000) musman->downloadSong(id);
            // else {
            //     musman->downloadCustomSong(id);
            // }

            _filesTotal++;
        }
        for (int id : effects) {
            if (musman->isSFXDownloaded(id)) continue;

            if (!added_as_delegate) {
                musman->addMusicDownloadDelegate(this);
                added_as_delegate = true;
            }

            log::info("downloading sfx {}", id);
            // musman->downloadSFX(id);

            _filesTotal++;
        }

        if (_filesTotal != 0) {
            updateTitle();
        }

        if (st->level->m_levelString.size() < 256) {
            log::error("level string is STILL EMPTY. WTF?");

        } else {
            _playBtn->setVisible(true);
        }

        // This fixes the song being unknown? its weird but it works so who cares
        auto songObject = st->level->m_songID == 0 ? LevelTools::getSongObject(st->level->m_audioTrack) : SongInfoObject::create(st->level->m_songID);
        auto songWidget = CustomSongWidget::create(songObject, nullptr, false, false, true, st->level->m_songID == 0, false, false, 0);
        _widget = songWidget;

        songWidget->updateSongInfo();
        songWidget->updateWithMultiAssets(st->level->m_songIDs, st->level->m_sfxIDs, 0);
        songWidget->setVisible(false);

        this->addChild(songWidget);

        if (songWidget->m_getSongInfoBtn->isVisible()) {
            songWidget->m_getSongInfoBtn->activate();
            log::info("getting music info (data is unknown)");
        }
        schedule(schedule_selector(LevelProgressionPopup::waitForMusicInfo), 0.5f);
    }

    void checkDownloaded(float d) {
        auto glm = GameLevelManager::get();
        auto st = getState();
        if (!st->level) {
            log::info("predownloaded level is nullptr");
            return;
        }
        if (glm->hasDownloadedLevel(st->level->m_levelID)) {
            auto level = glm->getSavedLevel(st->level->m_levelID);
            if (!level) {
                log::info("LEVEL IS NULLPTR. ignoring");
                return;
            }
            if (level->m_levelString.size() < 256) {
                log::info("NO LEVEL STRING. ignoring");
                return;
            }
            processOnDownloaed();
            unschedule(schedule_selector(LevelProgressionPopup::checkDownloaded));
        } else {
            log::info("not yet");
        }
    }

    void setupLevelPage(const std::string &response) {
        if (_currentCell != nullptr) {
            _currentCell->removeMeAndCleanup();
            _currentCell = nullptr;
        }

        GJGameLevel *level = GJGameLevel::create();

        auto leveljson = nlohmann::json::parse(response);

        long long seed = 0;
        PARSE_INT(seed, leveljson["origSeed"]);

        auto st = getState();

        if (!st->seedSet) {
            st->seed = std::to_string(seed);
            log::info("setting seed to {}", st->seed);
            st->seedSet = true;
        }

        PARSE_STRING(level->m_levelName, leveljson["levelName"]);
        PARSE_STRING(level->m_levelDesc, leveljson["levelDescription"]);
        PARSE_STRING(level->m_uploadDate, leveljson["publicationDate"]);
        PARSE_STRING(level->m_creatorName, leveljson["username"]);
        // PARSE_STRING(level->m_songIDs, leveljson["song_ids"]);
        // PARSE_STRING(level->m_sfxIDs, leveljson["sfx_ids"]);
        PARSE_INT(level->m_audioTrack, leveljson["musicID"]);
        PARSE_INT(level->m_gameVersion, leveljson["fakeGameVersion"]);
        PARSE_INT(level->m_ratings, leveljson["difficulty_denominator"]);
        PARSE_INT(level->m_ratingsSum, leveljson["difficulty_numerator"]);
        PARSE_INT(level->m_gameVersion, leveljson["fakeGameVersion"]);
        // PARSE_INT(level->m_ratings, leveljson["rating"]);
        // PARSE_INT(level->m_ratingsSum, leveljson["rating_sum"]);
        PARSE_INT(level->m_downloads, leveljson["downloads"]);
        PARSE_INT(level->m_likes, leveljson["likes"]);
        PARSE_INT(level->m_levelLength, leveljson["length"]);
        PARSE_INT(level->m_userID, leveljson["playerID"]);
        PARSE_INT(level->m_coins, leveljson["coins"]);
        ///PARSE_INT(level->m_coinsVerified, leveljson["coins_verified"]);
        level->m_coinsVerified = 1;
        PARSE_INT(level->m_rateStars, leveljson["stars"]);
        PARSE_INT(level->m_stars, leveljson["stars"]);
        PARSE_INT(level->m_starsRequested, leveljson["starsRequested"]);
        PARSE_INT(level->m_accountID, leveljson["accountID"]);
        PARSE_INT(level->m_levelID, leveljson["levelID"]);
        PARSE_BOOL(level->m_demon, leveljson["isDemon"]);
        PARSE_BOOL(level->m_autoLevel, leveljson["isAuto"]);
        //PARSE_BOOL(level->m_isEditable, leveljson["level_string_available"]);
        level->m_isEditable = false;
        PARSE_INT(level->m_demonDifficulty, leveljson["demonDifficulty"]);
        // PARSE_INT(level->m_demonVotes, leveljson["id"]);
        PARSE_INT(level->m_featured, leveljson["featureScore"]);
        PARSE_INT(level->m_songID, leveljson["songID"]);
        PARSE_INT(level->m_isEpic, leveljson["isEpic"]);

        level->retain();

        auto csz = getRealContentSize();
        // log::info("1");

        // LevelCell *cell = LevelCell::create(0.f, 0.f);
        // log::info("2");
        // cell->loadFromLevel(level);
        // log::info("3");
        // cell->setPosition(0, csz.height / 2);
        // log::info("4");
        LevelCell *cell = new LevelCell("a", 0.f, 0.f);
        // log::info("2");
        if (!cell->init()) return;
        // log::info("3");
        cell->autorelease();
        // log::info("4");
        cell->loadFromLevel(level);
        // log::info("5");
        cell->setPosition(0, csz.height / 2);

        CCLayer *base = typeinfo_cast<CCLayer *>(cell->getChildByID("main-layer"));
        base->setAnchorPoint({0.15, 0});
        base->setPositionX(20.f);
        base->setScale(0.925f);

        std::vector<CCNode *> to_lower;

        to_lower.push_back(cell->getChildByIDRecursive("length-label"));
        to_lower.push_back(cell->getChildByIDRecursive("downloads-label"));
        to_lower.push_back(cell->getChildByIDRecursive("likes-label"));
        to_lower.push_back(cell->getChildByIDRecursive("length-icon"));
        to_lower.push_back(cell->getChildByIDRecursive("downloads-icon"));
        to_lower.push_back(cell->getChildByIDRecursive("likes-icon"));
        to_lower.push_back(cell->getChildByIDRecursive("orbs-icon"));
        to_lower.push_back(cell->getChildByIDRecursive("orbs-label"));
        to_lower.push_back(cell->getChildByIDRecursive("moons-icon"));
        to_lower.push_back(cell->getChildByIDRecursive("moons-label"));

        int idx = 0;

        for (auto node : to_lower) {
            if (node != nullptr) {
                node->setPositionY(node->getPositionY() - 4);
            } else {
                log::info("node at {} is nullptr", idx);
            }
        }

        idx++;

        CCLabelBMFont *song_name = typeinfo_cast<CCLabelBMFont *>(base->getChildByID("song-name"));
        CCMenuItemSpriteExtra *creator_name = typeinfo_cast<CCMenuItemSpriteExtra *>(base->getChildByIDRecursive("creator-name"));
        song_name->setPositionY(song_name->getPositionY() - 15);
        creator_name->setPositionY(creator_name->getPositionY() - 15);

        m_mainLayer->addChild(cell, 100);

        // CCLabelBMFont *dateLabel = CCLabelBMFont::create(level->m_uploadDate.c_str(), "chatFont.fnt");
        // dateLabel->setScale(0.5f);
        // dateLabel->setPosition(csz.width / 2, -37);

        // cell->addChild(dateLabel);
        //

        CCMenuItemSpriteExtra *viewBtn = typeinfo_cast<CCMenuItemSpriteExtra *>(cell->getChildByIDRecursive("view-button"));
        viewBtn->setVisible(false);

        _circle->setVisible(false);

        CCNode *obj1 = cell->getChildByIDRecursive("Level-Ratings/ratings-label");
        CCNode *obj2 = cell->getChildByIDRecursive("Level-Ratings/ratings-icon");
        CCNode *obj3 = cell->getChildByIDRecursive("cvolton.betterinfo/level-id-label");

        if (obj1 != nullptr) {
            obj1->removeMeAndCleanup();
        }
        if (obj2 != nullptr) {
            obj2->removeMeAndCleanup();
        }
        if (obj3 != nullptr) {
            obj3->removeMeAndCleanup();
        }

        st->level = level;
        st->cell = cell;

        auto glm = GameLevelManager::get();
        GameLevelManager::get()->downloadLevel(st->level->m_levelID, false);
        // // glm->deleteLevel(glm->getSavedLevel(st->level->m_levelID));
        // if (!glm->hasDownloadedLevel(st->level->m_levelID)) {
        //     log::info("level is not downloaded - downloading it");
            schedule(schedule_selector(LevelProgressionPopup::checkDownloaded), 0.5f);
            _playBtn->setVisible(false);
        // } else {
        //     auto saved = glm->getSavedLevel(st->level->m_levelID);
        //     saved->m_creatorName = st->level->m_creatorName;
        //     saved->m_accountID = st->level->m_accountID;
        //     st->level->m_levelString = saved->m_levelString;
        //     st->level->m_songIDs = saved->m_songIDs;
        //     st->level->m_sfxIDs = saved->m_sfxIDs;

        //     // st->level = saved;
        //     if (st->level->m_levelString.size() < 256) {
        //         log::info("level has empty level string - downloading it");
        //         // GameLevelManager::get()->deleteLevel(saved);
        //         GameLevelManager::get()->downloadLevel(st->level->m_levelID, false);
        //         schedule(schedule_selector(LevelProgressionPopup::checkDownloaded), 0.5f);
        //         _playBtn->setVisible(false);
        //         return;
        //     } else {
        //         st->level->m_songIDs = saved->m_songIDs;
        //         st->level->m_sfxIDs = saved->m_sfxIDs;
        //     }
        // }
        // // log::info("levelString={}", st->level->m_levelString);

        // if (st->level->m_levelString.size() < 256) {
        //     _playBtn->setVisible(false);
        //     log::info("level has empty level string (2) - downloading it");
        //     GameLevelManager::get()->downloadLevel(st->level->m_levelID, false);
        //     schedule(schedule_selector(LevelProgressionPopup::checkDownloaded), 0.5f);
        // }

        _currentCell = cell;
        _circle->setVisible(false);
    }

    void fetchLevel() {
        if (_currentCell != nullptr) {
            _currentCell->removeMeAndCleanup();
            _currentCell = nullptr;
        }

        _circle->setVisible(true);
        if (_playBtn) _playBtn->setVisible(false);

        std::map<LevelProgressionState::Difficulty, std::string> m1 = {
            {LevelProgressionState::SDEasy, "std_easy"},
            {LevelProgressionState::SDNormal, "std_normal"},
            {LevelProgressionState::SDHard, "std_hard"},
            {LevelProgressionState::SDHarder, "std_harder"},
            {LevelProgressionState::SDInsane, "std_insane"},
        };
        std::map<LevelProgressionState::DemonDiff, std::string> m2 = {
            {LevelProgressionState::SDDEasy, "demon_easy"},
            {LevelProgressionState::SDDMedium, "demon_medium"},
            {LevelProgressionState::SDDHard, "demon_hard"},
            {LevelProgressionState::SDDInsane, "demon_insane"},
            {LevelProgressionState::SDDExtreme, "demon_extreme"},
        };

        auto st = getState();

        std::string d = "";
        if (st->demon_dif == LevelProgressionState::DemonDiff::SDDNone) {
            d = m1[st->std_dif];
        } else {
            d = m2[st->demon_dif];
        }

        std::string url = "https://www.dogotrigger.ru/lp/get/" + d;
        url += "?user_id=" + std::to_string(GameManager::get()->m_playerUserID);
        if (st->seedSet) {
            url += "&seed=" + st->seed;
        }

        log::info("url={}", url);

        st->net->setURL(url);
        st->net->setMethod(GeodeNetwork::MGet);
        st->net->setOkCallback([this, st](GeodeNetwork*net) {
           log::info("ok response = {}", st->net->getResponse());
           setupLevelPage(st->net->getResponse());
        });
        st->net->setErrorCallback([this](GeodeNetwork*net) {
            scheduleOnce(schedule_selector(LevelProgressionPopup::retryFetch), 0.1f);
        });
        st->net->send();
    }

    bool setup() override {
        LPGlobal::curPopup = this;

        setTitle("Difficulty Progression");
        m_mainLayer->setID("main-layer");
        setID("LevelProgressionPopup");

        auto st = getState();
        st->init();

        CCSprite *spr = CCSprite::createWithSpriteFrameName("GJ_optionsBtn02_001.png");
        CCMenuItemSpriteExtra *e = CCMenuItemSpriteExtra::create(spr, spr, menu_selector(LevelProgressionPopup::onShowSeed));

        CCMenu *btns = CCMenu::create();
        btns->setContentSize(getRealContentSize());
        btns->setPosition({0,0});
        btns->addChild(e);
        m_mainLayer->addChild(btns);
        _buttons = btns;

        CCMenu *m = CCMenu::create();
        m->setContentSize(getRealContentSize());
        m->setAnchorPoint({0.f, 0.5});
        RowLayout *layout = RowLayout::create();
        // layout->setAutoScale(false);
        // layout->setGrowCrossAxis(false);
        // layout->setCrossAxisOverflow(false);
        layout->setGap(10.f);
        m->setLayout(layout);
        // m_mainLayer->addChildAtPosition(m, Anchor::Center);
        m_mainLayer->addChild(m, 100);

        // char const* caption, int width, int p2, float scale, bool absolute, char const* font, char const* bg, float height
        auto spr4 = ButtonSprite::create("Play");
        spr4->setScale(0.8f);

        CCMenuItemSpriteExtra *btn = CCMenuItemSpriteExtra::create(spr4, spr4, this, menu_selector(LevelProgressionPopup::playLevel));
        m->addChild(btn);
        m->addChild(LevelProgressionLives::create(st));
        m->updateLayout();
        m->setPosition({0, m->getContentHeight() / 2 + 8.f});
        btn->setVisible(false);
        _playBtn = btn;

        if (st->clickedBegin) {
            onClickBegin(nullptr);
        } else {
            auto spr4 = ButtonSprite::create("Begin");
            CCMenuItemSpriteExtra *btn = CCMenuItemSpriteExtra::create(spr4, spr4, this, menu_selector(LevelProgressionPopup::onClickBegin));
            btn->setPosition(getRealContentSize() / 2);
            btns->addChild(btn);
            btns->setTouchPriority(-1000);

            _extraNodes.push_back(btn);

            if (!Mod::get()->getSavedValue<bool>("pressed-seed-menu", false)) {
                auto recommendation = TextArea::create("Before playing you can set a <cy>seed</c>. Check it out!", "chatFont.fnt", 1.f, 240.f, {0.5, 0.5f}, 12.f, false);
                auto recommendation2 = TextArea::create("Before playing you can set a <cy>seed</c>. Check it out!", "chatFont.fnt", 1.f, 240.f, {0.5, 0.5f}, 12.f, false);
                auto csz = getRealContentSize();
                recommendation->setPositionX(csz.width / 2.f);
                recommendation->setPositionY(csz.height - recommendation->getContentHeight() - 40.f);
                float offset = 1.f;
                recommendation2->setPositionX(recommendation->getPositionX() + offset);
                recommendation2->setPositionY(recommendation->getPositionY() - offset);
                recommendation2->colorAllCharactersTo({0,0,0});
                m_mainLayer->addChild(recommendation2, 0);
                m_mainLayer->addChild(recommendation, 1);
                _extraNodes.push_back(recommendation);
                _extraNodes.push_back(recommendation2);
                btn->setPositionY(btn->getPositionY() - 10.f);
            }
        }

        return true;
    }

public:
    static LevelProgressionPopup* create() {
        auto ret = new LevelProgressionPopup();
        if (ret->initAnchored(320.f, 160.f)) {
            ret->autorelease();
            return ret;
        }

        delete ret;
        return nullptr;
    }

    LevelProgressionState *getState() {
        log::info("popup: getstate: {} {}", (void *)&LPGlobal::state, (void *)&LPGlobal::state);
        return &LPGlobal::state;
    }

    CCSize getRealContentSize() {
        return m_mainLayer->getContentSize();
    }

    void onShowSeed(CCObject *sender) {
        Mod::get()->setSavedValue<bool>("pressed-seed-menu", true);
        LevelProgressionRandPopup::create(getState())->show();
    }
    void retryFetch(float d) {
        log::info("attempting to fetch level again");
        fetchLevel();
    }
    void playLevel(CCObject *) {
        auto st = getState();

        if (!st->cell || !st->level) return;

        if (st->level->m_levelString.size() < 256) {
            log::info("DATA1={}", st->level->m_levelString);
            std::string s = _cachedLevelString;
            s.resize(256);
            log::info("DATA2={}", s);
            st->level->m_levelString = _cachedLevelString;
        }

        LPGlobal::state.beganGameover = false;
        LPGlobal::state.lives++;

        auto fmod = FMODAudioEngine::get();
        fmod->stopAllMusic(true);
        fmod->stopAllEffects();

        auto musman = MusicDownloadManager::sharedState();
        musman->removeMusicDownloadDelegate(this);

        auto scene = PlayLayer::scene(st->level, true, true);
        auto transition = CCTransitionFade::create(0.5f, scene);

        CCDirector::sharedDirector()->replaceScene(transition);
    }
    void onClickBegin(CCObject*) {
        auto st = getState();
        st->clickedBegin = true;
        _circle = LoadingCircleLayer::create();
        m_mainLayer->addChildAtPosition(_circle, Anchor::Center);

        fetchLevel();
        if (_buttons) {
            // _buttons->setVisible(false);
        }
        for (CCNode *nd : _extraNodes) {
            if (!nd) continue;
            nd->setVisible(false);
        }
    }

    void animateIn() {
        this->m_noElasticity = true;
        this->show();

        auto winSize = CCDirector::get()->getWinSize();
        auto targetPos = winSize / 2;
        this->setScale(1.f);

        float old_y = this->m_mainLayer->getPositionY();
        this->m_mainLayer->setPositionY(old_y + (winSize.height / 2) + (getRealContentSize().height / 2));
        this->m_mainLayer->runAction(CCEaseBounceOut::create(CCMoveTo::create(1.f, {this->m_mainLayer->getPositionX(), old_y})));

        this->setOpacity(0);
        this->runAction(CCFadeTo::create(0.5f, 128));
    }
};

class $modify(HPlayLayer, PlayLayer) {
    struct Fields {
		float player_x_old;
		float player_x_new;
		float player_x_delta;

		bool levelStarted = false;
		bool runningAnimation = false;

		bool died_flag = false;

		LevelProgressionLives *overlay = nullptr;
	};

	void printValues(float d) {
	   // log::info("{} {} {} {}", LPGlobal::state.began, LPGlobal::state.in_playlayer, LPGlobal::state.level, LPGlobal::overlay);
	}

	void fixBegan(float d) {
	    log::info("fixing lp state");
	    LPGlobal::state.began = true;
		LPGlobal::state.in_playlayer = true;
		LPGlobal::overlay = m_fields->overlay;
	}

	void onExit() {
	    log::info("onExit()");

		PlayLayer::onExit();
	}

	bool isPlayerDead() {
		return m_player1->m_isDead || m_player2->m_isDead;
	}

	void updateVisibility(float delta) {
		PlayLayer::updateVisibility(delta);

		m_fields->player_x_old = m_fields->player_x_new;
		m_fields->player_x_new = m_player1->getPositionX();

		m_fields->player_x_delta = m_fields->player_x_new - m_fields->player_x_old;

		if (m_fields->player_x_delta != 0.f) m_fields->levelStarted = true;

		if (isPlayerDead()) {
			if (!m_fields->died_flag && LPGlobal::state.began) {
			    // LPGlobal::overlay->decreaseLives();
			}
			m_fields->died_flag = true;
		}
	}

	void setupGameover(float delta) {
	    LPGlobal::state = {};
	    LPGlobal::state.std_dif = LevelProgressionState::SDEasy;
		LPGlobal::state.demon_dif = LevelProgressionState::SDDNone;
		LevelProgressionPopup::create()->show();
#ifdef _WIN32
        CCDirector::get()->getOpenGLView()->showCursor(true);
#endif
	}

	void beginGameover() {
		log::info("beginGameover()");

		if (LPGlobal::state.beganGameover) return;

		LPGlobal::state.beganGameover = true;

		const auto& winSize = CCDirector::sharedDirector()->getWinSize();

		auto base = CCSprite::create("square.png");
		base->setPosition({ 0, 0 });
		base->setScale(500.f);
		base->setColor({0, 0, 0});
		base->setOpacity(0);
		base->runAction(CCFadeTo::create(1.f, 255));

		addChild(base, 9999);

		scheduleOnce(schedule_selector(HPlayLayer::setupGameover), 1.f);

		unscheduleUpdate();
	}

	void resetLevel() {
		if (isPlayerDead() && LPGlobal::state.lives <= 0 && LPGlobal::state.began) {
			beginGameover();
		} else {
		    m_fields->died_flag = false;
			int atts = this->m_attempts;
			PlayLayer::resetLevel();
			int atts2 = this->m_attempts;
			if (LPGlobal::state.began) {
   if (atts != atts2) {
			   LPGlobal::overlay->decreaseLives();
			}
			if (LPGlobal::state.lives <= 0) {
                HPlayLayer *pl = (HPlayLayer*)PlayLayer::get();
                pl->beginGameover();
            }
			}
		}
	}
	void resetLevelFromStart() {
	    log::info("resetLevelFromStart()");
		PlayLayer::resetLevelFromStart();
	}

	void fullReset() {
		log::info("FullReset()");
		PlayLayer::fullReset();
	}

    void showLPLayer() {
        auto node = getChildByIDRecursive("EndLevelLayer");
        if (node) {
            node->setVisible(false);
        } else {
            log::error("could not find endlevellayer");
        }

        LPGlobal::state.cur_l++;
        if (LPGlobal::state.cur_l >= LPGlobal::state.lpd) {
            LPGlobal::state.cur_l = 0;

            int v = (int)LPGlobal::state.std_dif;
            if (v == LevelProgressionState::SDInsane) {
                int v2 = (int)LPGlobal::state.demon_dif;
                if (v2 < LevelProgressionState::SDDExtreme) {
                    v2++;
                    LPGlobal::state.demon_dif = (LevelProgressionState::DemonDiff)v2;
                }
            } else {
                v++;
                LPGlobal::state.std_dif = (LevelProgressionState::Difficulty)v;
            }
        }

        LevelProgressionPopup::create()->animateIn();
        FMODAudioEngine::get()->fadeOutMusic(1.f, 0);
    }
    void showCompleteText() {
        PlayLayer::showCompleteText();

        log::info("{}", LPGlobal::state.began);

        if (!LPGlobal::state.began) return;

        auto d = CCDelayTime::create(1.5f);
        auto f = CCCallFunc::create(this, callfunc_selector(HPlayLayer::showLPLayer));

        float move_by = LPGlobal::overlay->getContentHeight() + LPGlobal::overlay->getPositionY();
        LPGlobal::overlay->runAction(CCEaseExponentialIn::create(CCMoveBy::create(1.5f, {0.f, -move_by})));

        runAction(CCSequence::create(d, f, nullptr));
    }
    bool init(GJGameLevel *level, bool p1, bool p2) {
        if (p1 && p2) {
            scheduleOnce(schedule_selector(HPlayLayer::fixBegan), 3.f);
            p1 = false;
            p2 = false;
            LPGlobal::state.began = true;
        } else {
            LPGlobal::overlay = nullptr;
            LPGlobal::state.cell = nullptr;
            LPGlobal::state.in_playlayer = false;
            LPGlobal::state.began = false;
            //
            // scheduleOnce(schedule_selector(HPlayLayer::fixBegan), 3.f);
            // p1 = false;
            // p2 = false;
            // LPGlobal::state.began = true;
        }

        if (!PlayLayer::init(level, p1, p2)) return false;
        if (!LPGlobal::state.began) return true;

        schedule(schedule_selector(HPlayLayer::printValues), 0.5f);

        LPGlobal::state.level = level;

        LevelProgressionLives *overlay = LevelProgressionLives::create(&LPGlobal::state);
        overlay->setScale(0.875f);
        overlay->setPosition({4,4});
        addChild(overlay, 100);
        m_fields->overlay = overlay;

        LPGlobal::state.in_playlayer = true;

        fixBegan(0.f);

        return true;
    }
};

class $modify(HCreatorLayer, CreatorLayer) {
    void prInit(CCObject *sender) {
        LevelProgressionPopup::create()->show();
    }

    bool init() {
        if (!CreatorLayer::init()) return false;

        // CCLabelBMFont *l = CCLabelBMFont::create("P", "bigFont.fnt");
        // l->setAlignment(CCTextAlignment::kCCTextAlignmentCenter);
        // l->updateLabel();
        // CircleButtonSprite *b = CircleButtonSprite::create(l);
        CCSprite *b = CCSprite::createWithSpriteFrameName("dfbtn.png"_spr);
        b->setScale(0.6f);
        b->setAnchorPoint({0.5f, 0.5f});
        auto sz = CCDirector::sharedDirector()->getWinSize();

        CCMenu *lm = CCMenu::create();
        CCMenuItemSpriteExtra *btn = CCMenuItemSpriteExtra::create(b, b, this, menu_selector(HCreatorLayer::prInit));
        btn->setAnchorPoint({0.25f, 0.5f});

        lm->setContentSize(btn->getContentSize());
        lm->setPosition({19.f, sz.height / 2});

        lm->addChild(btn);

        addChild(lm);

        return true;
    }
};

class $modify(EndLevelLayer) {
    void coinEnterFinished(CCPoint p) {
        log::info("coinEnterFinished: {}", p);
        if (LPGlobal::overlay != nullptr) {
            LPGlobal::overlay->increaseLives();
        }
        if (auto p = PlayLayer::get()->getChildByIDRecursive("lp-overlay")) {
            LevelProgressionLives *pl = typeinfo_cast<LevelProgressionLives*>(pl);
            if (!pl) {
                log::error("cannot convert pointer {} to LevelProgressionLives", p);
            } else {
                pl->getState()->lives--;
                pl->increaseLives();
            }
        } else {
            log::error("cannot find lp-overlay in playlayer");
        }
        EndLevelLayer::coinEnterFinished(p);
    }
    void coinEnterFinishedO(CCObject *p) {
        log::info("coinEnterFinished0: {}", p);
        EndLevelLayer::coinEnterFinishedO(p);
    }
    void playCoinEffect(float p) {
        log::info("playCoinEffect: {}", p);
        EndLevelLayer::playCoinEffect(p);
    }
};

class $modify(CCBlockLayer) {
    bool init() {
        CCBlockLayer::init();

        // if (LPGlobal::state.beganGameover) {
        //     this->setVisible(false);
        //     this->setTouchEnabled(false);
        //     this->setKeypadEnabled(false);
        //     this->setKeyboardEnabled(false);
        // }

        return true;

    }
};

class $modify(PauseLayer) {
    void onRestart(CCObject *sender) {
        if (LPGlobal::state.beganGameover) return;
        PauseLayer::onRestart(sender);

        if (LPGlobal::overlay != nullptr && LPGlobal::state.began) {
            if (LPGlobal::state.lives <= 0) {
                HPlayLayer *pl = (HPlayLayer*)PlayLayer::get();
                pl->beginGameover();
            } else {
                // LPGlobal::overlay->decreaseLives();
                PauseLayer::onRestart(sender);

                if (LPGlobal::state.lives <= 0) {
                    HPlayLayer *pl = (HPlayLayer*)PlayLayer::get();
                    pl->beginGameover();
                }
            }
        }
    }
    void onRestartFull(CCObject *sender) {
        if (LPGlobal::state.beganGameover) return;
        PauseLayer::onRestartFull(sender);
        if (LPGlobal::overlay != nullptr && LPGlobal::state.began) {
            if (LPGlobal::state.lives <= 0) {
                HPlayLayer *pl = (HPlayLayer*)PlayLayer::get();
                pl->beginGameover();
            } else {
                // LPGlobal::overlay->decreaseLives();
                PauseLayer::onRestart(sender);

                if (LPGlobal::state.lives <= 0) {
                    HPlayLayer *pl = (HPlayLayer*)PlayLayer::get();
                    pl->beginGameover();
                }
            }
        }
    }
    void onQuit(CCObject *sender) {
        if (Mod::get()->getSettingValue<bool>("reset-lives-after-quit")) {
            LPGlobal::state.lives = Mod::get()->getSettingValue<int>("lives");
        }
        LPGlobal::state.seed.clear();
        LPGlobal::state.seedSet = false;
        PauseLayer::onQuit(sender);
    }
};

void LevelProgressionRandPopup::onClose(CCObject *o) {
    auto s = _seedInput->getString();
    if (s != _inputBefore && LPGlobal::curPopup != nullptr) {
        LPGlobal::curPopup->fetchLevel();
    }
    geode::Popup<LevelProgressionState*>::onClose(o);
}
