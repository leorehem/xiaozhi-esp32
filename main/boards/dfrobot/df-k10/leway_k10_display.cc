#include "leway_k10_display.h"

#include "display/lvgl_display/lvgl_theme.h"
#include <esp_log.h>
#include <cmath>
#include <cstring>

#define TAG "LewayK10Display"

namespace {
constexpr uint32_t kBgFreijoHex = 0x2B1E16;
constexpr uint32_t kCardDarkHex = 0x211610;
constexpr uint32_t kTextCreamHex = 0xF5EFE6;
constexpr uint32_t kTextMutedHex = 0xD4C6B4;
constexpr uint32_t kEyeDarkHex = 0x1D130D;
}  // namespace

LewayK10Display::LewayK10Display(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                                 int width, int height, int offset_x, int offset_y,
                                 bool mirror_x, bool mirror_y, bool swap_xy)
    : SpiLcdDisplay(panel_io, panel, width, height, offset_x, offset_y, mirror_x, mirror_y,
                    swap_xy) {}

LewayK10Display::~LewayK10Display() {
    if (anim_timer_ != nullptr) {
        lv_timer_delete(anim_timer_);
        anim_timer_ = nullptr;
    }
}

void LewayK10Display::SetupUI() {
    if (setup_ui_called_) {
        return;
    }
    Display::SetupUI();
    DisplayLockGuard lock(this);

    auto* lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    const lv_font_t* text_font =
        (lvgl_theme && lvgl_theme->text_font() && lvgl_theme->text_font()->font())
            ? lvgl_theme->text_font()->font()
            : LV_FONT_DEFAULT;
    const lv_font_t* icon_font =
        (lvgl_theme && lvgl_theme->icon_font() && lvgl_theme->icon_font()->font())
            ? lvgl_theme->icon_font()->font()
            : LV_FONT_DEFAULT;

    lv_obj_t* screen = lv_screen_active();
    lv_obj_clean(screen);
    lv_obj_set_style_bg_color(screen, lv_color_hex(kBgFreijoHex), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(screen, text_font, 0);
    lv_obj_set_style_text_color(screen, lv_color_hex(kTextCreamHex), 0);
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);

    // Container principal 240x320
    leway_root_ = lv_obj_create(screen);
    container_ = leway_root_;
    lv_obj_set_size(leway_root_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(leway_root_, lv_color_hex(kBgFreijoHex), 0);
    lv_obj_set_style_bg_opa(leway_root_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(leway_root_, 0, 0);
    lv_obj_set_style_radius(leway_root_, 0, 0);
    lv_obj_set_style_pad_all(leway_root_, 0, 0);
    lv_obj_set_scrollbar_mode(leway_root_, LV_SCROLLBAR_MODE_OFF);

    // ------------------------------------------------------------------
    // 1. TOPO DISCRETO: Marca LEWAY + Wi-Fi + Status curto
    // ------------------------------------------------------------------
    top_bar_ = lv_obj_create(leway_root_);
    lv_obj_set_size(top_bar_, LV_HOR_RES, 28);
    lv_obj_align(top_bar_, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(top_bar_, lv_color_hex(kCardDarkHex), 0);
    lv_obj_set_style_bg_opa(top_bar_, LV_OPA_70, 0);
    lv_obj_set_style_border_width(top_bar_, 0, 0);
    lv_obj_set_style_radius(top_bar_, 0, 0);
    lv_obj_set_style_pad_hor(top_bar_, 10, 0);
    lv_obj_set_style_pad_ver(top_bar_, 0, 0);
    lv_obj_set_scrollbar_mode(top_bar_, LV_SCROLLBAR_MODE_OFF);

    header_brand_label_ = lv_label_create(top_bar_);
    lv_label_set_text(header_brand_label_, "LEWAY");
    lv_obj_set_style_text_color(header_brand_label_, lv_color_hex(accent_hex_), 0);
    lv_obj_set_style_text_letter_space(header_brand_label_, 2, 0);
    lv_obj_align(header_brand_label_, LV_ALIGN_LEFT_MID, 0, 0);

    status_label_ = lv_label_create(top_bar_);
    lv_label_set_text(status_label_, "Pronto");
    lv_obj_set_style_text_color(status_label_, lv_color_hex(kTextMutedHex), 0);
    lv_obj_align(status_label_, LV_ALIGN_CENTER, 8, 0);

    network_label_ = lv_label_create(top_bar_);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, icon_font, 0);
    lv_obj_set_style_text_color(network_label_, lv_color_hex(kTextMutedHex), 0);
    lv_obj_align(network_label_, LV_ALIGN_RIGHT_MID, 0, 0);

    // Labels opcionais esperados pela classe base LvglDisplay (mantidos ocultos/discretos)
    notification_label_ = lv_label_create(leway_root_);
    lv_label_set_text(notification_label_, "");
    lv_obj_set_style_bg_color(notification_label_, lv_color_hex(kCardDarkHex), 0);
    lv_obj_set_style_bg_opa(notification_label_, LV_OPA_90, 0);
    lv_obj_set_style_text_color(notification_label_, lv_color_hex(kTextCreamHex), 0);
    lv_obj_set_style_pad_hor(notification_label_, 10, 0);
    lv_obj_set_style_pad_ver(notification_label_, 4, 0);
    lv_obj_set_style_radius(notification_label_, 10, 0);
    lv_obj_align(notification_label_, LV_ALIGN_TOP_MID, 0, 32);
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    mute_label_ = lv_label_create(top_bar_);
    lv_label_set_text(mute_label_, "");
    lv_obj_add_flag(mute_label_, LV_OBJ_FLAG_HIDDEN);

    battery_label_ = lv_label_create(top_bar_);
    lv_label_set_text(battery_label_, "");
    lv_obj_add_flag(battery_label_, LV_OBJ_FLAG_HIDDEN);

    // ------------------------------------------------------------------
    // 2. IDENTIDADE DA PRESENÇA (Nome + Origem sutil)
    // ------------------------------------------------------------------
    presence_name_label_ = lv_label_create(leway_root_);
    lv_label_set_text(presence_name_label_, "Ze Baiano");
    lv_obj_set_style_text_color(presence_name_label_, lv_color_hex(kTextCreamHex), 0);
    lv_obj_align(presence_name_label_, LV_ALIGN_TOP_MID, 0, 36);

    presence_sub_label_ = lv_label_create(leway_root_);
    lv_label_set_text(presence_sub_label_, "Itapua - Salvador");
    lv_obj_set_style_text_color(presence_sub_label_, lv_color_hex(accent_hex_), 0);
    lv_obj_set_style_text_opa(presence_sub_label_, LV_OPA_80, 0);
    lv_obj_align(presence_sub_label_, LV_ALIGN_TOP_MID, 0, 56);

    // ------------------------------------------------------------------
    // 3. PALCO CENTRAL: AURA ORGÂNICA LEWAY + OLHOS EXPRESSIVOS VIVOS
    // ------------------------------------------------------------------
    aura_stage_ = lv_obj_create(leway_root_);
    lv_obj_set_size(aura_stage_, 156, 156);
    lv_obj_align(aura_stage_, LV_ALIGN_TOP_MID, 0, 74);
    lv_obj_set_style_bg_opa(aura_stage_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(aura_stage_, 0, 0);
    lv_obj_set_style_pad_all(aura_stage_, 0, 0);
    lv_obj_set_scrollbar_mode(aura_stage_, LV_SCROLLBAR_MODE_OFF);

    // Anel externo (halo suave)
    aura_outer_ = lv_obj_create(aura_stage_);
    lv_obj_set_size(aura_outer_, 136, 136);
    lv_obj_center(aura_outer_);
    lv_obj_set_style_radius(aura_outer_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(aura_outer_, lv_color_hex(accent_hex_), 0);
    lv_obj_set_style_bg_opa(aura_outer_, LV_OPA_20, 0);
    lv_obj_set_style_border_width(aura_outer_, 2, 0);
    lv_obj_set_style_border_color(aura_outer_, lv_color_hex(accent_hex_), 0);
    lv_obj_set_style_border_opa(aura_outer_, LV_OPA_40, 0);
    lv_obj_set_scrollbar_mode(aura_outer_, LV_SCROLLBAR_MODE_OFF);

    // Anel médio (calor respirando)
    aura_mid_ = lv_obj_create(aura_stage_);
    lv_obj_set_size(aura_mid_, 106, 106);
    lv_obj_center(aura_mid_);
    lv_obj_set_style_radius(aura_mid_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(aura_mid_, lv_color_hex(accent_hex_), 0);
    lv_obj_set_style_bg_opa(aura_mid_, LV_OPA_40, 0);
    lv_obj_set_style_border_width(aura_mid_, 0, 0);
    lv_obj_set_scrollbar_mode(aura_mid_, LV_SCROLLBAR_MODE_OFF);

    // Núcleo central da Presença
    aura_core_ = lv_obj_create(aura_stage_);
    lv_obj_set_size(aura_core_, 78, 78);
    lv_obj_center(aura_core_);
    lv_obj_set_style_radius(aura_core_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(aura_core_, lv_color_hex(accent_hex_), 0);
    lv_obj_set_style_bg_opa(aura_core_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(aura_core_, 2, 0);
    lv_obj_set_style_border_color(aura_core_, lv_color_hex(kTextCreamHex), 0);
    lv_obj_set_style_border_opa(aura_core_, LV_OPA_50, 0);
    lv_obj_set_scrollbar_mode(aura_core_, LV_SCROLLBAR_MODE_OFF);

    // Olhos minimalistas expressivos dentro do núcleo
    eye_left_ = lv_obj_create(aura_core_);
    lv_obj_set_size(eye_left_, 12, 18);
    lv_obj_set_style_radius(eye_left_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(eye_left_, lv_color_hex(kEyeDarkHex), 0);
    lv_obj_set_style_bg_opa(eye_left_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(eye_left_, 0, 0);
    lv_obj_align(eye_left_, LV_ALIGN_CENTER, -14, -2);

    eye_right_ = lv_obj_create(aura_core_);
    lv_obj_set_size(eye_right_, 12, 18);
    lv_obj_set_style_radius(eye_right_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(eye_right_, lv_color_hex(kEyeDarkHex), 0);
    lv_obj_set_style_bg_opa(eye_right_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(eye_right_, 0, 0);
    lv_obj_align(eye_right_, LV_ALIGN_CENTER, 14, -2);

    // ------------------------------------------------------------------
    // 4. CARD DE LEGENDA ESTILO CINEMA NA BASE (sem bolhas feias!)
    // ------------------------------------------------------------------
    subtitle_card_ = lv_obj_create(leway_root_);
    lv_obj_set_size(subtitle_card_, LV_HOR_RES - 16, 80);
    lv_obj_align(subtitle_card_, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_style_bg_color(subtitle_card_, lv_color_hex(kCardDarkHex), 0);
    lv_obj_set_style_bg_opa(subtitle_card_, LV_OPA_90, 0);
    lv_obj_set_style_radius(subtitle_card_, 14, 0);
    lv_obj_set_style_border_width(subtitle_card_, 1, 0);
    lv_obj_set_style_border_color(subtitle_card_, lv_color_hex(accent_hex_), 0);
    lv_obj_set_style_border_opa(subtitle_card_, LV_OPA_40, 0);
    lv_obj_set_style_pad_hor(subtitle_card_, 10, 0);
    lv_obj_set_style_pad_ver(subtitle_card_, 6, 0);
    lv_obj_set_scrollbar_mode(subtitle_card_, LV_SCROLLBAR_MODE_OFF);

    subtitle_role_label_ = lv_label_create(subtitle_card_);
    lv_label_set_text(subtitle_role_label_, "PRESENCA");
    lv_obj_set_style_text_color(subtitle_role_label_, lv_color_hex(accent_hex_), 0);
    lv_obj_align(subtitle_role_label_, LV_ALIGN_TOP_LEFT, 0, 0);

    subtitle_text_label_ = lv_label_create(subtitle_card_);
    chat_message_label_ = subtitle_text_label_;
    lv_obj_set_width(subtitle_text_label_, LV_HOR_RES - 38);
    lv_label_set_long_mode(subtitle_text_label_, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(subtitle_text_label_, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_color(subtitle_text_label_, lv_color_hex(kTextCreamHex), 0);
    lv_label_set_text(subtitle_text_label_, "Aperte o botao ou chame para conversar.");
    lv_obj_align(subtitle_text_label_, LV_ALIGN_TOP_LEFT, 0, 18);

    // Timer de animação suave a 20 FPS (50ms)
    anim_timer_ = lv_timer_create(AnimTimerCb, 50, this);
    ApplyPresenceTheme("ze_baiano");
}

void LewayK10Display::SetStatus(const char* status) {
    if (!setup_ui_called_ || status == nullptr) return;
    DisplayLockGuard lock(this);

    if (notification_label_ != nullptr) {
        lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);
    }
    if (status_label_ != nullptr) {
        lv_obj_remove_flag(status_label_, LV_OBJ_FLAG_HIDDEN);
    }

    std::string st(status);
    // Traduz/simplifica estados padrão do firmware para Português elegante
    if (st.find("Speaking") != std::string::npos || st.find("说话") != std::string::npos) {
        voice_state_ = VoiceState::Speaking;
        if (status_label_) lv_label_set_text(status_label_, "Falando");
    } else if (st.find("Listening") != std::string::npos || st.find("聆听") != std::string::npos) {
        voice_state_ = VoiceState::Listening;
        if (status_label_) lv_label_set_text(status_label_, "Ouvindo...");
    } else if (st.find("Connecting") != std::string::npos || st.find("连接") != std::string::npos) {
        voice_state_ = VoiceState::Idle;
        if (status_label_) lv_label_set_text(status_label_, "Conectando...");
    } else if (st.find("Standby") != std::string::npos || st.find("待命") != std::string::npos) {
        voice_state_ = VoiceState::Idle;
        if (status_label_) lv_label_set_text(status_label_, "Em espera");
    } else {
        voice_state_ = VoiceState::Idle;
        if (status_label_) lv_label_set_text(status_label_, status);
    }
    last_status_update_time_ = std::chrono::system_clock::now();
}

bool LewayK10Display::SetEmotion(const char* emotion) {
    if (emotion == nullptr) return false;
    DisplayLockGuard lock(this);

    std::string emo(emotion);

    // Protocolo LeWay: "presence:<id>" troca instantaneamente paleta, nome e LEDs RGB da K10!
    constexpr const char* kPresencePrefix = "presence:";
    if (emo.rfind(kPresencePrefix, 0) == 0) {
        ApplyPresenceTheme(emo.substr(std::strlen(kPresencePrefix)));
        return true;
    }

    if (emo == "laughing" || emo == "funny" || emo == "silly") {
        current_mood_ = VisualMood::Laughing;
    } else if (emo == "happy" || emo == "cool" || emo == "confident" || emo == "winking") {
        current_mood_ = VisualMood::Happy;
    } else if (emo == "thinking" || emo == "confused") {
        current_mood_ = VisualMood::Thinking;
    } else if (emo == "surprised" || emo == "shocked") {
        current_mood_ = VisualMood::Surprised;
    } else if (emo == "sad" || emo == "crying") {
        current_mood_ = VisualMood::Sad;
    } else if (emo == "loving" || emo == "kissy") {
        current_mood_ = VisualMood::Loving;
    } else if (emo == "sleepy" || emo == "relaxed") {
        current_mood_ = VisualMood::Sleepy;
    } else {
        current_mood_ = VisualMood::Neutral;
    }

    ApplyMoodGeometry(is_blinking_);
    return true;
}

void LewayK10Display::SetChatMessage(const char* role, const char* content) {
    if (content == nullptr) return;
    DisplayLockGuard lock(this);

    if (subtitle_role_label_ != nullptr) {
        bool is_user = (role != nullptr && std::strcmp(role, "user") == 0);
        lv_label_set_text(subtitle_role_label_, is_user ? "VOCE" : "FALANDO");
        lv_obj_set_style_text_color(
            subtitle_role_label_,
            lv_color_hex(is_user ? kTextMutedHex : accent_hex_),
            0);
    }

    if (subtitle_text_label_ != nullptr) {
        std::string text(content);
        if (text.empty()) {
            lv_label_set_text(subtitle_text_label_, "...");
            return;
        }
        // Mantém no máximo ~110 bytes para leitura limpa em 3 linhas na tela 240x320
        if (text.size() > 110) {
            text = text.substr(text.size() - 107);
            auto space_pos = text.find(' ');
            if (space_pos != std::string::npos && space_pos < 24) {
                text = text.substr(space_pos + 1);
            }
            text = "..." + text;
        }
        lv_label_set_text(subtitle_text_label_, text.c_str());
    }
}

void LewayK10Display::RemoveChatMessage(const char* /*role*/, const char* /*content*/) {
    // Legenda estilo cinema exibe apenas a fala atual; nada a remover
}

void LewayK10Display::ClearChatMessages() {
    if (!setup_ui_called_) return;
    DisplayLockGuard lock(this);
    if (subtitle_text_label_ != nullptr) {
        lv_label_set_text(subtitle_text_label_, "Aperte o botao ou chame para conversar.");
    }
}

void LewayK10Display::SetPreviewImage(std::unique_ptr<LvglImage> /*image*/) {
    // Tela LeWay prioriza a Aura e a legenda limpa; ignora preview sem risco de ponteiro nulo
}

void LewayK10Display::SetHideSubtitle(bool hide) {
    hide_subtitle_ = hide;
    if (!setup_ui_called_) return;
    DisplayLockGuard lock(this);
    if (subtitle_card_ != nullptr) {
        if (hide) {
            lv_obj_add_flag(subtitle_card_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_remove_flag(subtitle_card_, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void LewayK10Display::SetTheme(Theme* theme) {
    if (theme == nullptr) return;
    DisplayLockGuard lock(this);

    // Mantém a identidade Freijó LeWay intacta e atualiza apenas fontes carregadas pelos Assets,
    // evitando que LcdDisplay::SetTheme tente acessar emoji_label_/content_ (nulos neste layout).
    current_theme_ = theme;
    auto* lvgl_theme = static_cast<LvglTheme*>(theme);

    const lv_font_t* text_font =
        (lvgl_theme->text_font() && lvgl_theme->text_font()->font())
            ? lvgl_theme->text_font()->font()
            : nullptr;
    const lv_font_t* icon_font =
        (lvgl_theme->icon_font() && lvgl_theme->icon_font()->font())
            ? lvgl_theme->icon_font()->font()
            : nullptr;

    lv_obj_t* screen = lv_screen_active();
    if (text_font != nullptr && screen != nullptr) {
        lv_obj_set_style_text_font(screen, text_font, 0);
        if (leway_root_) lv_obj_set_style_text_font(leway_root_, text_font, 0);
        if (subtitle_text_label_) lv_obj_set_style_text_font(subtitle_text_label_, text_font, 0);
    }
    if (icon_font != nullptr) {
        if (network_label_) lv_obj_set_style_text_font(network_label_, icon_font, 0);
        if (mute_label_) lv_obj_set_style_text_font(mute_label_, icon_font, 0);
        if (battery_label_) lv_obj_set_style_text_font(battery_label_, icon_font, 0);
    }
}

void LewayK10Display::ApplyPresenceTheme(const std::string& presence_id) {
    static const PresenceTheme kThemes[] = {
        {"dora", "Dora", "Vila Matilde - SP", 0x8FA876, 30, 50, 22},
        {"ze_baiano", "Ze Baiano", "Itapua - Salvador", 0xE69D45, 65, 38, 8},
        {"jorge", "Jorge Carioca", "Arpoador - Rio", 0x2D938B, 10, 52, 48},
        {"papai_noel", "Papai Noel", "Oficina de Pinho", 0xC86D51, 68, 22, 14},
        {"saci", "Saci", "Redemoinho", 0xD95D39, 70, 20, 8},
        {"emilia", "Emilia", "Boneca Inventora", 0xD86A8E, 65, 18, 36},
    };

    const PresenceTheme* selected = &kThemes[1];  // Ze Baiano default
    for (const auto& t : kThemes) {
        if (presence_id == t.id) {
            selected = &t;
            break;
        }
    }

    accent_hex_ = selected->accent_hex;
    lv_color_t accent = lv_color_hex(accent_hex_);

    if (header_brand_label_) lv_obj_set_style_text_color(header_brand_label_, accent, 0);
    if (presence_name_label_) lv_label_set_text(presence_name_label_, selected->display_name);
    if (presence_sub_label_) {
        lv_label_set_text(presence_sub_label_, selected->origin_hint);
        lv_obj_set_style_text_color(presence_sub_label_, accent, 0);
    }
    if (aura_outer_) {
        lv_obj_set_style_bg_color(aura_outer_, accent, 0);
        lv_obj_set_style_border_color(aura_outer_, accent, 0);
    }
    if (aura_mid_) lv_obj_set_style_bg_color(aura_mid_, accent, 0);
    if (aura_core_) lv_obj_set_style_bg_color(aura_core_, accent, 0);
    if (subtitle_card_) lv_obj_set_style_border_color(subtitle_card_, accent, 0);
    if (subtitle_role_label_) lv_obj_set_style_text_color(subtitle_role_label_, accent, 0);

    if (rgb_cb_) {
        rgb_cb_(selected->led_r, selected->led_g, selected->led_b);
    }
    ESP_LOGI(TAG, "Tema LeWay aplicado: %s (#%06lX)", selected->display_name,
             static_cast<unsigned long>(accent_hex_));
}

void LewayK10Display::ApplyMoodGeometry(bool blinking) {
    if (!eye_left_ || !eye_right_) return;

    if (blinking) {
        lv_obj_set_size(eye_left_, 14, 3);
        lv_obj_set_size(eye_right_, 14, 3);
        lv_obj_align(eye_left_, LV_ALIGN_CENTER, -14, 0);
        lv_obj_align(eye_right_, LV_ALIGN_CENTER, 14, 0);
        return;
    }

    switch (current_mood_) {
        case VisualMood::Laughing:
        case VisualMood::Happy:
            lv_obj_set_size(eye_left_, 14, 14);
            lv_obj_set_size(eye_right_, 14, 14);
            lv_obj_align(eye_left_, LV_ALIGN_CENTER, -14, -4);
            lv_obj_align(eye_right_, LV_ALIGN_CENTER, 14, -4);
            break;
        case VisualMood::Surprised:
            lv_obj_set_size(eye_left_, 15, 22);
            lv_obj_set_size(eye_right_, 15, 22);
            lv_obj_align(eye_left_, LV_ALIGN_CENTER, -14, -2);
            lv_obj_align(eye_right_, LV_ALIGN_CENTER, 14, -2);
            break;
        case VisualMood::Thinking:
            lv_obj_set_size(eye_left_, 12, 16);
            lv_obj_set_size(eye_right_, 12, 11);
            lv_obj_align(eye_left_, LV_ALIGN_CENTER, -11, -5);
            lv_obj_align(eye_right_, LV_ALIGN_CENTER, 17, -4);
            break;
        case VisualMood::Sad:
            lv_obj_set_size(eye_left_, 12, 10);
            lv_obj_set_size(eye_right_, 12, 10);
            lv_obj_align(eye_left_, LV_ALIGN_CENTER, -14, 4);
            lv_obj_align(eye_right_, LV_ALIGN_CENTER, 14, 4);
            break;
        case VisualMood::Sleepy:
            lv_obj_set_size(eye_left_, 14, 5);
            lv_obj_set_size(eye_right_, 14, 5);
            lv_obj_align(eye_left_, LV_ALIGN_CENTER, -14, 2);
            lv_obj_align(eye_right_, LV_ALIGN_CENTER, 14, 2);
            break;
        case VisualMood::Loving:
        case VisualMood::Neutral:
        default:
            lv_obj_set_size(eye_left_, 12, 18);
            lv_obj_set_size(eye_right_, 12, 18);
            lv_obj_align(eye_left_, LV_ALIGN_CENTER, -14, -2);
            lv_obj_align(eye_right_, LV_ALIGN_CENTER, 14, -2);
            break;
    }
}

void LewayK10Display::UpdateAuraSizes(int outer_d, int mid_d, int core_d, lv_opa_t outer_opa,
                                      lv_opa_t mid_opa) {
    if (aura_outer_) {
        lv_obj_set_size(aura_outer_, outer_d, outer_d);
        lv_obj_set_style_bg_opa(aura_outer_, outer_opa, 0);
        lv_obj_center(aura_outer_);
    }
    if (aura_mid_) {
        lv_obj_set_size(aura_mid_, mid_d, mid_d);
        lv_obj_set_style_bg_opa(aura_mid_, mid_opa, 0);
        lv_obj_center(aura_mid_);
    }
    if (aura_core_) {
        lv_obj_set_size(aura_core_, core_d, core_d);
        lv_obj_center(aura_core_);
    }
}

void LewayK10Display::AnimTimerCb(lv_timer_t* timer) {
    auto* self = static_cast<LewayK10Display*>(lv_timer_get_user_data(timer));
    if (self != nullptr) {
        self->TickAnimation();
    }
}

void LewayK10Display::TickAnimation() {
    anim_tick_ += 1;

    // Piscar orgânico a cada ~3.5 segundos
    if (!is_blinking_ && anim_tick_ >= next_blink_tick_) {
        is_blinking_ = true;
        ApplyMoodGeometry(true);
    } else if (is_blinking_ && anim_tick_ >= next_blink_tick_ + 3) {
        is_blinking_ = false;
        next_blink_tick_ = anim_tick_ + 55 + (anim_tick_ % 35);
        ApplyMoodGeometry(false);
    }

    float t = static_cast<float>(anim_tick_) * 0.12f;
    if (voice_state_ == VoiceState::Speaking) {
        // Pulso vivo da Aura enquanto a Presença fala
        float wave = 0.5f + 0.5f * std::sin(t * 2.6f) * std::cos(t * 0.9f);
        int outer_d = 128 + static_cast<int>(wave * 24.0f);
        int mid_d = 100 + static_cast<int>(wave * 16.0f);
        int core_d = 76 + static_cast<int>(wave * 8.0f);
        UpdateAuraSizes(outer_d, mid_d, core_d, LV_OPA_30, LV_OPA_50);
    } else if (voice_state_ == VoiceState::Listening) {
        // Respiração atenta e calma enquanto escuta você
        float breath = 0.5f + 0.5f * std::sin(t * 1.1f);
        int outer_d = 126 + static_cast<int>(breath * 14.0f);
        int mid_d = 98 + static_cast<int>(breath * 10.0f);
        int core_d = 76 + static_cast<int>(breath * 4.0f);
        UpdateAuraSizes(outer_d, mid_d, core_d, LV_OPA_20, LV_OPA_40);
    } else {
        // Repouso suave orgânico
        float rest = 0.5f + 0.5f * std::sin(t * 0.55f);
        int outer_d = 120 + static_cast<int>(rest * 8.0f);
        int mid_d = 94 + static_cast<int>(rest * 6.0f);
        UpdateAuraSizes(outer_d, mid_d, 74, LV_OPA_10, LV_OPA_30);
    }
}
