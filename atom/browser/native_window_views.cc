// Copyright (c) 2014 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "atom/browser/native_window_views.h"

#include <string>
#include <vector>

#include "atom/browser/ui/views/menu_bar.h"
#include "atom/browser/ui/views/menu_layout.h"
#include "atom/common/draggable_region.h"
#include "atom/common/options_switches.h"
#include "base/strings/utf_string_conversions.h"
#include "brightray/browser/inspectable_web_contents.h"
#include "brightray/browser/inspectable_web_contents_view.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "native_mate/dictionary.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/image/image.h"
#include "ui/views/background.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/window/client_view.h"
#include "ui/views/widget/native_widget_private.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/shadow_types.h"

#if defined(USE_X11)
#include "atom/browser/browser.h"
#include "atom/browser/ui/views/global_menu_bar_x11.h"
#include "atom/browser/ui/views/frameless_view.h"
#include "atom/browser/ui/views/native_frame_view.h"
#include "atom/browser/ui/x/window_state_watcher.h"
#include "atom/browser/ui/x/x_window_utils.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ui/libgtk2ui/unity_service.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/views/window/native_frame_view.h"
#elif defined(OS_WIN)
#include "atom/browser/ui/views/win_frame_view.h"
#include "atom/browser/ui/win/atom_desktop_window_tree_host_win.h"
#include "ui/base/win/shell.h"
#include "ui/gfx/win/dpi.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#endif

namespace atom {

namespace {

// The menu bar height in pixels.
#if defined(OS_WIN)
const int kMenuBarHeight = 20;
#else
const int kMenuBarHeight = 25;
#endif

bool IsAltKey(const content::NativeWebKeyboardEvent& event) {
#if defined(USE_X11)
  // 164 and 165 represent VK_LALT and VK_RALT.
  return event.windowsKeyCode == 164 || event.windowsKeyCode == 165;
#else
  return event.windowsKeyCode == ui::VKEY_MENU;
#endif
}

bool IsAltModifier(const content::NativeWebKeyboardEvent& event) {
  typedef content::NativeWebKeyboardEvent::Modifiers Modifiers;
  int modifiers = event.modifiers;
  modifiers &= ~Modifiers::NumLockOn;
  modifiers &= ~Modifiers::CapsLockOn;
  return (modifiers == Modifiers::AltKey) ||
         (modifiers == (Modifiers::AltKey | Modifiers::IsLeft)) ||
         (modifiers == (Modifiers::AltKey | Modifiers::IsRight));
}

#if defined(OS_WIN)
// Convert Win32 WM_APPCOMMANDS to strings.
const char* AppCommandToString(int command_id) {
  switch (command_id) {
    case APPCOMMAND_BROWSER_BACKWARD       : return "browser-backward";
    case APPCOMMAND_BROWSER_FORWARD        : return "browser-forward";
    case APPCOMMAND_BROWSER_REFRESH        : return "browser-refresh";
    case APPCOMMAND_BROWSER_STOP           : return "browser-stop";
    case APPCOMMAND_BROWSER_SEARCH         : return "browser-search";
    case APPCOMMAND_BROWSER_FAVORITES      : return "browser-favorites";
    case APPCOMMAND_BROWSER_HOME           : return "browser-home";
    case APPCOMMAND_VOLUME_MUTE            : return "volume-mute";
    case APPCOMMAND_VOLUME_DOWN            : return "volume-down";
    case APPCOMMAND_VOLUME_UP              : return "volume-up";
    case APPCOMMAND_MEDIA_NEXTTRACK        : return "media-nexttrack";
    case APPCOMMAND_MEDIA_PREVIOUSTRACK    : return "media-previoustrack";
    case APPCOMMAND_MEDIA_STOP             : return "media-stop";
    case APPCOMMAND_MEDIA_PLAY_PAUSE       : return "media-play_pause";
    case APPCOMMAND_LAUNCH_MAIL            : return "launch-mail";
    case APPCOMMAND_LAUNCH_MEDIA_SELECT    : return "launch-media-select";
    case APPCOMMAND_LAUNCH_APP1            : return "launch-app1";
    case APPCOMMAND_LAUNCH_APP2            : return "launch-app2";
    case APPCOMMAND_BASS_DOWN              : return "bass-down";
    case APPCOMMAND_BASS_BOOST             : return "bass-boost";
    case APPCOMMAND_BASS_UP                : return "bass-up";
    case APPCOMMAND_TREBLE_DOWN            : return "treble-down";
    case APPCOMMAND_TREBLE_UP              : return "treble-up";
    case APPCOMMAND_MICROPHONE_VOLUME_MUTE : return "microphone-volume-mute";
    case APPCOMMAND_MICROPHONE_VOLUME_DOWN : return "microphone-volume-down";
    case APPCOMMAND_MICROPHONE_VOLUME_UP   : return "microphone-volume-up";
    case APPCOMMAND_HELP                   : return "help";
    case APPCOMMAND_FIND                   : return "find";
    case APPCOMMAND_NEW                    : return "new";
    case APPCOMMAND_OPEN                   : return "open";
    case APPCOMMAND_CLOSE                  : return "close";
    case APPCOMMAND_SAVE                   : return "save";
    case APPCOMMAND_PRINT                  : return "print";
    case APPCOMMAND_UNDO                   : return "undo";
    case APPCOMMAND_REDO                   : return "redo";
    case APPCOMMAND_COPY                   : return "copy";
    case APPCOMMAND_CUT                    : return "cut";
    case APPCOMMAND_PASTE                  : return "paste";
    case APPCOMMAND_REPLY_TO_MAIL          : return "reply-to-mail";
    case APPCOMMAND_FORWARD_MAIL           : return "forward-mail";
    case APPCOMMAND_SEND_MAIL              : return "send-mail";
    case APPCOMMAND_SPELL_CHECK            : return "spell-check";
    case APPCOMMAND_MIC_ON_OFF_TOGGLE      : return "mic-on-off-toggle";
    case APPCOMMAND_CORRECTION_LIST        : return "correction-list";
    case APPCOMMAND_MEDIA_PLAY             : return "media-play";
    case APPCOMMAND_MEDIA_PAUSE            : return "media-pause";
    case APPCOMMAND_MEDIA_RECORD           : return "media-record";
    case APPCOMMAND_MEDIA_FAST_FORWARD     : return "media-fast-forward";
    case APPCOMMAND_MEDIA_REWIND           : return "media-rewind";
    case APPCOMMAND_MEDIA_CHANNEL_UP       : return "media-channel-up";
    case APPCOMMAND_MEDIA_CHANNEL_DOWN     : return "media-channel-down";
    case APPCOMMAND_DELETE                 : return "delete";
    case APPCOMMAND_DICTATE_OR_COMMAND_CONTROL_TOGGLE:
      return "dictate-or-command-control-toggle";
    default:
      return "unknown";
  }
}
#endif

class NativeWindowClientView : public views::ClientView {
 public:
  NativeWindowClientView(views::Widget* widget,
                         NativeWindowViews* contents_view)
      : views::ClientView(widget, contents_view) {
  }
  virtual ~NativeWindowClientView() {}

  bool CanClose() override {
    static_cast<NativeWindowViews*>(contents_view())->RequestToClosePage();
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeWindowClientView);
};

}  // namespace

NativeWindowViews::NativeWindowViews(
    brightray::InspectableWebContents* web_contents,
    const mate::Dictionary& options)
    : NativeWindow(web_contents, options),
      window_(new views::Widget),
      web_view_(inspectable_web_contents()->GetView()->GetView()),
      menu_bar_autohide_(false),
      menu_bar_visible_(false),
      menu_bar_alt_pressed_(false),
      keyboard_event_handler_(new views::UnhandledKeyboardEventHandler),
      use_content_size_(false),
      resizable_(true) {
  options.Get(switches::kTitle, &title_);
  options.Get(switches::kAutoHideMenuBar, &menu_bar_autohide_);

#if defined(OS_WIN)
  // On Windows we rely on the CanResize() to indicate whether window can be
  // resized, and it should be set before window is created.
  options.Get(switches::kResizable, &resizable_);
#endif

  if (enable_larger_than_screen())
    // We need to set a default maximum window size here otherwise Windows
    // will not allow us to resize the window larger than scree.
    // Setting directly to INT_MAX somehow doesn't work, so we just devide
    // by 10, which should still be large enough.
    maximum_size_.SetSize(INT_MAX / 10, INT_MAX / 10);

  int width = 800, height = 600;
  options.Get(switches::kWidth, &width);
  options.Get(switches::kHeight, &height);
  gfx::Rect bounds(0, 0, width, height);
  widget_size_ = bounds.size();

  window_->AddObserver(this);

  views::Widget::InitParams params;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = bounds;
  params.delegate = this;
  params.type = views::Widget::InitParams::TYPE_WINDOW;
  params.remove_standard_frame = !has_frame();

  if (transparent())
    params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;

#if defined(OS_WIN)
  params.native_widget =
      new views::DesktopNativeWidgetAura(window_.get());
  atom_desktop_window_tree_host_win_ = new AtomDesktopWindowTreeHostWin(
      this,
      window_.get(),
      static_cast<views::DesktopNativeWidgetAura*>(params.native_widget));
  params.desktop_window_tree_host = atom_desktop_window_tree_host_win_;
#elif defined(USE_X11)
  std::string name = Browser::Get()->GetName();
  // Set WM_WINDOW_ROLE.
  params.wm_role_name = "browser-window";
  // Set WM_CLASS.
  params.wm_class_name = base::StringToLowerASCII(name);
  params.wm_class_class = name;
#endif

  window_->Init(params);

  bool fullscreen = false;
  options.Get(switches::kFullscreen, &fullscreen);

#if defined(USE_X11)
  // Start monitoring window states.
  window_state_watcher_.reset(new WindowStateWatcher(this));

  // Set _GTK_THEME_VARIANT to dark if we have "dark-theme" option set.
  bool use_dark_theme = false;
  if (options.Get(switches::kDarkTheme, &use_dark_theme) && use_dark_theme) {
    XDisplay* xdisplay = gfx::GetXDisplay();
    XChangeProperty(xdisplay, GetAcceleratedWidget(),
                    XInternAtom(xdisplay, "_GTK_THEME_VARIANT", False),
                    XInternAtom(xdisplay, "UTF8_STRING", False),
                    8, PropModeReplace,
                    reinterpret_cast<const unsigned char*>("dark"),
                    4);
  }

  // Before the window is mapped the SetWMSpecState can not work, so we have
  // to manually set the _NET_WM_STATE.
  std::vector<::Atom> state_atom_list;
  bool skip_taskbar = false;
  if (options.Get(switches::kSkipTaskbar, &skip_taskbar) && skip_taskbar) {
    state_atom_list.push_back(GetAtom("_NET_WM_STATE_SKIP_TASKBAR"));
  }

  // Before the window is mapped, there is no SHOW_FULLSCREEN_STATE.
  if (fullscreen) {
    state_atom_list.push_back(GetAtom("_NET_WM_STATE_FULLSCREEN"));
  }

  ui::SetAtomArrayProperty(GetAcceleratedWidget(), "_NET_WM_STATE", "ATOM",
                           state_atom_list);

  // Set the _NET_WM_WINDOW_TYPE.
  std::string window_type;
  if (options.Get(switches::kType, &window_type))
    SetWindowType(GetAcceleratedWidget(), window_type);
#endif

  // Add web view.
  SetLayoutManager(new MenuLayout(this, kMenuBarHeight));
  set_background(views::Background::CreateStandardPanelBackground());
  AddChildView(web_view_);

  if (has_frame() &&
      options.Get(switches::kUseContentSize, &use_content_size_) &&
      use_content_size_)
    bounds = ContentBoundsToWindowBounds(bounds);

#if defined(OS_WIN)
  // Save initial window state.
  if (fullscreen)
    last_window_state_ = ui::SHOW_STATE_FULLSCREEN;
  else
    last_window_state_ = ui::SHOW_STATE_NORMAL;

  last_normal_size_ = gfx::Size(widget_size_);

  if (!has_frame()) {
    // Set Window style so that we get a minimize and maximize animation when
    // frameless.
    DWORD frame_style = WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX |
                        WS_CAPTION;
    // We should not show a frame for transparent window.
    if (transparent())
      frame_style &= ~(WS_THICKFRAME | WS_CAPTION);
    ::SetWindowLong(GetAcceleratedWidget(), GWL_STYLE, frame_style);
  }

  if (transparent()) {
    // Transparent window on Windows has to have WS_EX_COMPOSITED style.
    LONG ex_style = ::GetWindowLong(GetAcceleratedWidget(), GWL_EXSTYLE);
    ex_style |= WS_EX_COMPOSITED;
    ::SetWindowLong(GetAcceleratedWidget(), GWL_EXSTYLE, ex_style);
  }
#endif

  // TODO(zcbenz): This was used to force using native frame on Windows 2003, we
  // should check whether setting it in InitParams can work.
  if (has_frame()) {
    window_->set_frame_type(views::Widget::FrameType::FRAME_TYPE_FORCE_NATIVE);
    window_->FrameTypeChanged();
  }

  // The given window is most likely not rectangular since it uses
  // transparency and has no standard frame, don't show a shadow for it.
  if (transparent() && !has_frame())
    wm::SetShadowType(GetNativeWindow(), wm::SHADOW_TYPE_NONE);

  window_->UpdateWindowIcon();
  window_->CenterWindow(bounds.size());
  Layout();
}

NativeWindowViews::~NativeWindowViews() {
  window_->RemoveObserver(this);
}

void NativeWindowViews::Close() {
  window_->Close();
}

void NativeWindowViews::CloseImmediately() {
  window_->CloseNow();
}

void NativeWindowViews::Focus(bool focus) {
  if (focus)
    window_->Activate();
  else
    window_->Deactivate();
}

bool NativeWindowViews::IsFocused() {
  return window_->IsActive();
}

void NativeWindowViews::Show() {
  window_->native_widget_private()->ShowWithWindowState(GetRestoredState());
}

void NativeWindowViews::ShowInactive() {
  window_->ShowInactive();
}

void NativeWindowViews::Hide() {
  window_->Hide();
}

bool NativeWindowViews::IsVisible() {
  return window_->IsVisible();
}

void NativeWindowViews::Maximize() {
  if (IsVisible())
    window_->Maximize();
  else
    window_->native_widget_private()->ShowWithWindowState(
        ui::SHOW_STATE_MAXIMIZED);
}

void NativeWindowViews::Unmaximize() {
  window_->Restore();
}

bool NativeWindowViews::IsMaximized() {
  return window_->IsMaximized();
}

void NativeWindowViews::Minimize() {
  if (IsVisible())
    window_->Minimize();
  else
    window_->native_widget_private()->ShowWithWindowState(
        ui::SHOW_STATE_MINIMIZED);
}

void NativeWindowViews::Restore() {
  window_->Restore();
}

bool NativeWindowViews::IsMinimized() {
  return window_->IsMinimized();
}

void NativeWindowViews::SetFullScreen(bool fullscreen) {
#if defined(OS_WIN)
  // There is no native fullscreen state on Windows.
  if (fullscreen) {
    last_window_state_ = ui::SHOW_STATE_FULLSCREEN;
    NotifyWindowEnterFullScreen();
  } else {
    last_window_state_ = ui::SHOW_STATE_NORMAL;
    NotifyWindowLeaveFullScreen();
  }
  // We set the new value after notifying, so we can handle the size event
  // correctly.
  window_->SetFullscreen(fullscreen);
#else
  if (IsVisible())
    window_->SetFullscreen(fullscreen);
  else
    window_->native_widget_private()->ShowWithWindowState(
        ui::SHOW_STATE_FULLSCREEN);
#endif
}

bool NativeWindowViews::IsFullscreen() const {
  return window_->IsFullscreen();
}

void NativeWindowViews::SetBounds(const gfx::Rect& bounds) {
#if defined(USE_X11)
  // On Linux the minimum and maximum size should be updated with window size
  // when window is not resizable.
  if (!resizable_) {
    SetMaximumSize(bounds.size());
    SetMinimumSize(bounds.size());
  }
#endif

  window_->SetBounds(bounds);
}

gfx::Rect NativeWindowViews::GetBounds() {
#if defined(OS_WIN)
  if (IsMinimized())
    return window_->GetRestoredBounds();
#endif

  return window_->GetWindowBoundsInScreen();
}

void NativeWindowViews::SetContentSize(const gfx::Size& size) {
  if (!has_frame()) {
    NativeWindow::SetSize(size);
    return;
  }

  gfx::Rect bounds = window_->GetWindowBoundsInScreen();
  bounds.set_size(size);
  SetBounds(ContentBoundsToWindowBounds(bounds));
}

gfx::Size NativeWindowViews::GetContentSize() {
  if (!has_frame())
    return GetSize();

  gfx::Size content_size =
      window_->non_client_view()->frame_view()->GetBoundsForClientView().size();
  if (menu_bar_ && menu_bar_visible_)
    content_size.set_height(content_size.height() - kMenuBarHeight);
  return content_size;
}

void NativeWindowViews::SetMinimumSize(const gfx::Size& size) {
  minimum_size_ = size;
}

gfx::Size NativeWindowViews::GetMinimumSize() {
  return minimum_size_;
}

void NativeWindowViews::SetMaximumSize(const gfx::Size& size) {
  maximum_size_ = size;
}

gfx::Size NativeWindowViews::GetMaximumSize() {
  return maximum_size_;
}

void NativeWindowViews::SetResizable(bool resizable) {
#if defined(OS_WIN)
  // WS_MAXIMIZEBOX => Maximize button
  // WS_MINIMIZEBOX => Minimize button
  // WS_THICKFRAME => Resize handle
  DWORD style = ::GetWindowLong(GetAcceleratedWidget(), GWL_STYLE);
  if (resizable)
    style |= WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_THICKFRAME;
  else
    style = (style & ~(WS_MAXIMIZEBOX | WS_THICKFRAME)) | WS_MINIMIZEBOX;
  ::SetWindowLong(GetAcceleratedWidget(), GWL_STYLE, style);
#elif defined(USE_X11)
  if (resizable != resizable_) {
    // On Linux there is no "resizable" property of a window, we have to set
    // both the minimum and maximum size to the window size to achieve it.
    if (resizable) {
      SetMaximumSize(gfx::Size());
      SetMinimumSize(gfx::Size());
    } else {
      SetMaximumSize(GetSize());
      SetMinimumSize(GetSize());
    }
  }
#endif

  resizable_ = resizable;
}

bool NativeWindowViews::IsResizable() {
  return resizable_;
}

void NativeWindowViews::SetAlwaysOnTop(bool top) {
  window_->SetAlwaysOnTop(top);
}

bool NativeWindowViews::IsAlwaysOnTop() {
  return window_->IsAlwaysOnTop();
}

void NativeWindowViews::Center() {
  window_->CenterWindow(GetSize());
}

void NativeWindowViews::SetTitle(const std::string& title) {
  title_ = title;
  window_->UpdateWindowTitle();
}

std::string NativeWindowViews::GetTitle() {
  return title_;
}

void NativeWindowViews::FlashFrame(bool flash) {
#if defined(OS_WIN)
  // The Chromium's implementation has a bug stopping flash.
  if (!flash) {
    FLASHWINFO fwi;
    fwi.cbSize = sizeof(fwi);
    fwi.hwnd = GetAcceleratedWidget();
    fwi.dwFlags = FLASHW_STOP;
    fwi.uCount = 0;
    FlashWindowEx(&fwi);
    return;
  }
#endif
  window_->FlashFrame(flash);
}

void NativeWindowViews::SetSkipTaskbar(bool skip) {
#if defined(OS_WIN)
  base::win::ScopedComPtr<ITaskbarList> taskbar;
  if (FAILED(taskbar.CreateInstance(CLSID_TaskbarList, NULL,
                                    CLSCTX_INPROC_SERVER)) ||
      FAILED(taskbar->HrInit()))
    return;
  if (skip)
    taskbar->DeleteTab(GetAcceleratedWidget());
  else
    taskbar->AddTab(GetAcceleratedWidget());
#elif defined(USE_X11)
  SetWMSpecState(GetAcceleratedWidget(), skip,
                 GetAtom("_NET_WM_STATE_SKIP_TASKBAR"));
#endif
}

void NativeWindowViews::SetKiosk(bool kiosk) {
  SetFullScreen(kiosk);
}

bool NativeWindowViews::IsKiosk() {
  return IsFullscreen();
}

void NativeWindowViews::SetMenu(ui::MenuModel* menu_model) {
  if (menu_model == nullptr) {
    // Remove accelerators
    accelerator_table_.clear();
    GetFocusManager()->UnregisterAccelerators(this);
    // and menu bar.
#if defined(USE_X11)
    global_menu_bar_.reset();
#endif
    SetMenuBarVisibility(false);
    menu_bar_.reset();
    return;
  }

  RegisterAccelerators(menu_model);

#if defined(USE_X11)
  if (!global_menu_bar_ && ShouldUseGlobalMenuBar())
    global_menu_bar_.reset(new GlobalMenuBarX11(this));

  // Use global application menu bar when possible.
  if (global_menu_bar_ && global_menu_bar_->IsServerStarted()) {
    global_menu_bar_->SetMenu(menu_model);
    return;
  }
#endif

  // Do not show menu bar in frameless window.
  if (!has_frame())
    return;

  if (!menu_bar_) {
    gfx::Size content_size = GetContentSize();
    menu_bar_.reset(new MenuBar);
    menu_bar_->set_owned_by_client();

    if (!menu_bar_autohide_) {
      SetMenuBarVisibility(true);
      if (use_content_size_)
        SetContentSize(content_size);
    }
  }

  menu_bar_->SetMenu(menu_model);
  Layout();
}

gfx::NativeWindow NativeWindowViews::GetNativeWindow() {
  return window_->GetNativeWindow();
}

void NativeWindowViews::SetProgressBar(double progress) {
#if defined(OS_WIN)
  taskbar_host_.SetProgressBar(GetAcceleratedWidget(), progress);
#elif defined(USE_X11)
  if (unity::IsRunning()) {
    unity::SetProgressFraction(progress);
  }
#endif
}

void NativeWindowViews::SetOverlayIcon(const gfx::Image& overlay,
                                       const std::string& description) {
#if defined(OS_WIN)
  taskbar_host_.SetOverlayIcon(GetAcceleratedWidget(), overlay, description);
#endif
}

void NativeWindowViews::SetAutoHideMenuBar(bool auto_hide) {
  menu_bar_autohide_ = auto_hide;
}

bool NativeWindowViews::IsMenuBarAutoHide() {
  return menu_bar_autohide_;
}

void NativeWindowViews::SetMenuBarVisibility(bool visible) {
  if (!menu_bar_ || menu_bar_visible_ == visible)
    return;

  // Always show the accelerator when the auto-hide menu bar shows.
  if (menu_bar_autohide_)
    menu_bar_->SetAcceleratorVisibility(visible);

  menu_bar_visible_ = visible;
  if (visible) {
    DCHECK_EQ(child_count(), 1);
    AddChildView(menu_bar_.get());
  } else {
    DCHECK_EQ(child_count(), 2);
    RemoveChildView(menu_bar_.get());
  }

  Layout();
}

bool NativeWindowViews::IsMenuBarVisible() {
  return menu_bar_visible_;
}

void NativeWindowViews::SetVisibleOnAllWorkspaces(bool visible) {
  window_->SetVisibleOnAllWorkspaces(visible);
}

bool NativeWindowViews::IsVisibleOnAllWorkspaces() {
#if defined(USE_X11)
  // Use the presence/absence of _NET_WM_STATE_STICKY in _NET_WM_STATE to
  // determine whether the current window is visible on all workspaces.
  XAtom sticky_atom = GetAtom("_NET_WM_STATE_STICKY");
  std::vector<XAtom> wm_states;
  ui::GetAtomArrayProperty(GetAcceleratedWidget(), "_NET_WM_STATE", &wm_states);
  return std::find(wm_states.begin(),
                   wm_states.end(), sticky_atom) != wm_states.end();
#endif
  return false;
}

gfx::AcceleratedWidget NativeWindowViews::GetAcceleratedWidget() {
  return GetNativeWindow()->GetHost()->GetAcceleratedWidget();
}

void NativeWindowViews::OnWidgetActivationChanged(
    views::Widget* widget, bool active) {
  if (widget != window_.get())
    return;

  if (active)
    NotifyWindowFocus();
  else
    NotifyWindowBlur();

  if (active && inspectable_web_contents() &&
      !inspectable_web_contents()->IsDevToolsViewShowing())
    web_contents()->Focus();

  // Hide menu bar when window is blured.
  if (!active && menu_bar_autohide_ && menu_bar_visible_)
    SetMenuBarVisibility(false);
}

void NativeWindowViews::OnWidgetBoundsChanged(
    views::Widget* widget, const gfx::Rect& bounds) {
  if (widget != window_.get())
    return;

  if (widget_size_ != bounds.size()) {
    NotifyWindowResize();
    widget_size_ = bounds.size();
  }
}

void NativeWindowViews::DeleteDelegate() {
  NotifyWindowClosed();
}

views::View* NativeWindowViews::GetInitiallyFocusedView() {
  return inspectable_web_contents()->GetView()->GetWebView();
}

bool NativeWindowViews::CanResize() const {
  return resizable_;
}

bool NativeWindowViews::CanMaximize() const {
  return resizable_;
}

bool NativeWindowViews::CanMinimize() const {
  return true;
}

base::string16 NativeWindowViews::GetWindowTitle() const {
  return base::UTF8ToUTF16(title_);
}

bool NativeWindowViews::ShouldHandleSystemCommands() const {
  return true;
}

gfx::ImageSkia NativeWindowViews::GetWindowAppIcon() {
  return icon();
}

gfx::ImageSkia NativeWindowViews::GetWindowIcon() {
  return GetWindowAppIcon();
}

views::Widget* NativeWindowViews::GetWidget() {
  return window_.get();
}

const views::Widget* NativeWindowViews::GetWidget() const {
  return window_.get();
}

views::View* NativeWindowViews::GetContentsView() {
  return this;
}

bool NativeWindowViews::ShouldDescendIntoChildForEventHandling(
    gfx::NativeView child,
    const gfx::Point& location) {
  // App window should claim mouse events that fall within the draggable region.
  if (draggable_region() &&
      draggable_region()->contains(location.x(), location.y()))
    return false;

  // And the events on border for dragging resizable frameless window.
  if (!has_frame() && CanResize()) {
    FramelessView* frame = static_cast<FramelessView*>(
        window_->non_client_view()->frame_view());
    return frame->ResizingBorderHitTest(location) == HTNOWHERE;
  }

  return true;
}

views::ClientView* NativeWindowViews::CreateClientView(views::Widget* widget) {
  return new NativeWindowClientView(widget, this);
}

views::NonClientFrameView* NativeWindowViews::CreateNonClientFrameView(
    views::Widget* widget) {
#if defined(OS_WIN)
  WinFrameView* frame_view = new WinFrameView;
  frame_view->Init(this, widget);
  return frame_view;
#else
  if (has_frame()) {
    return new NativeFrameView(this, widget);
  } else {
    FramelessView* frame_view = new FramelessView;
    frame_view->Init(this, widget);
    return frame_view;
  }
#endif
}

void NativeWindowViews::OnWidgetMove() {
  NotifyWindowMove();
}

#if defined(OS_WIN)
bool NativeWindowViews::ExecuteWindowsCommand(int command_id) {
  std::string command = AppCommandToString(command_id);
  NotifyWindowExecuteWindowsCommand(command);
  return false;
}

bool NativeWindowViews::PreHandleMSG(
    UINT message, WPARAM w_param, LPARAM l_param, LRESULT* result) {
  switch (message) {
    case WM_COMMAND:
      // Handle thumbar button click message.
      if (HIWORD(w_param) == THBN_CLICKED)
        return taskbar_host_.HandleThumbarButtonEvent(LOWORD(w_param));
      return false;
    case WM_SIZE:
      // Handle window state change.
      HandleSizeEvent(w_param, l_param);
      return false;
    default:
      return false;
  }
}

void NativeWindowViews::HandleSizeEvent(WPARAM w_param, LPARAM l_param) {
  // Here we handle the WM_SIZE event in order to figure out what is the current
  // window state and notify the user accordingly.
  switch (w_param) {
    case SIZE_MAXIMIZED:
      last_window_state_ = ui::SHOW_STATE_MAXIMIZED;
      NotifyWindowMaximize();
      break;
    case SIZE_MINIMIZED:
      last_window_state_ = ui::SHOW_STATE_MINIMIZED;
      NotifyWindowMinimize();
      break;
    case SIZE_RESTORED:
      if (last_window_state_ == ui::SHOW_STATE_NORMAL) {
        // Window was resized so we save it's new size.
        last_normal_size_ = GetSize();
      } else {
        switch (last_window_state_) {
          case ui::SHOW_STATE_MAXIMIZED:
            last_window_state_ = ui::SHOW_STATE_NORMAL;

            // When the window is restored we resize it to the previous known
            // normal size.
            NativeWindow::SetSize(last_normal_size_);

            NotifyWindowUnmaximize();
            break;
          case ui::SHOW_STATE_MINIMIZED:
            if (IsFullscreen()) {
              last_window_state_ = ui::SHOW_STATE_FULLSCREEN;
              NotifyWindowEnterFullScreen();
            } else {
              last_window_state_ = ui::SHOW_STATE_NORMAL;

              // When the window is restored we resize it to the previous known
              // normal size.
              NativeWindow::SetSize(last_normal_size_);

              NotifyWindowRestore();
            }
            break;
        }
      }
      break;
  }
}
#endif

gfx::Size NativeWindowViews::WindowSizeToFramelessSize(
    const gfx::Size& size) {
  if (size.width() == 0 && size.height() == 0)
    return size;

  gfx::Rect window_bounds = gfx::Rect(size);
  if (use_content_size_) {
    if (menu_bar_ && menu_bar_visible_) {
      window_bounds.set_height(window_bounds.height() + kMenuBarHeight);
    }
  } else if (has_frame()) {
#if defined(OS_WIN)
  gfx::Size frame_size = gfx::win::ScreenToDIPRect(
      window_->non_client_view()->GetWindowBoundsForClientBounds(
          gfx::Rect())).size();
#else
  gfx::Size frame_size =
      window_->non_client_view()->GetWindowBoundsForClientBounds(
          gfx::Rect()).size();
#endif
    window_bounds.set_height(window_bounds.height() - frame_size.height());
    window_bounds.set_width(window_bounds.width() - frame_size.width());
  }

  return window_bounds.size();
}

void NativeWindowViews::HandleKeyboardEvent(
    content::WebContents*,
    const content::NativeWebKeyboardEvent& event) {
  keyboard_event_handler_->HandleKeyboardEvent(event, GetFocusManager());

  if (!menu_bar_)
    return;

  // Show accelerator when "Alt" is pressed.
  if (menu_bar_visible_ && IsAltKey(event))
    menu_bar_->SetAcceleratorVisibility(
        event.type == blink::WebInputEvent::RawKeyDown);

  // Show the submenu when "Alt+Key" is pressed.
  if (event.type == blink::WebInputEvent::RawKeyDown && !IsAltKey(event) &&
      IsAltModifier(event)) {
    if (!menu_bar_visible_ &&
        (menu_bar_->GetAcceleratorIndex(event.windowsKeyCode) != -1))
      SetMenuBarVisibility(true);
    menu_bar_->ActivateAccelerator(event.windowsKeyCode);
    return;
  }

  if (!menu_bar_autohide_)
    return;

  // Toggle the menu bar only when a single Alt is released.
  if (event.type == blink::WebInputEvent::RawKeyDown && IsAltKey(event)) {
    // When a single Alt is pressed:
    menu_bar_alt_pressed_ = true;
  } else if (event.type == blink::WebInputEvent::KeyUp && IsAltKey(event) &&
             menu_bar_alt_pressed_) {
    // When a single Alt is released right after a Alt is pressed:
    menu_bar_alt_pressed_ = false;
    SetMenuBarVisibility(!menu_bar_visible_);
  } else {
    // When any other keys except single Alt have been pressed/released:
    menu_bar_alt_pressed_ = false;
  }
}

bool NativeWindowViews::AcceleratorPressed(const ui::Accelerator& accelerator) {
  return accelerator_util::TriggerAcceleratorTableCommand(
      &accelerator_table_, accelerator);
}

void NativeWindowViews::RegisterAccelerators(ui::MenuModel* menu_model) {
  // Clear previous accelerators.
  views::FocusManager* focus_manager = GetFocusManager();
  accelerator_table_.clear();
  focus_manager->UnregisterAccelerators(this);

  // Register accelerators with focus manager.
  accelerator_util::GenerateAcceleratorTable(&accelerator_table_, menu_model);
  accelerator_util::AcceleratorTable::const_iterator iter;
  for (iter = accelerator_table_.begin();
       iter != accelerator_table_.end();
       ++iter) {
    focus_manager->RegisterAccelerator(
        iter->first, ui::AcceleratorManager::kNormalPriority, this);
  }
}

gfx::Rect NativeWindowViews::ContentBoundsToWindowBounds(
    const gfx::Rect& bounds) {
  gfx::Point origin = bounds.origin();
#if defined(OS_WIN)
  gfx::Rect dpi_bounds = gfx::win::DIPToScreenRect(bounds);
  gfx::Rect window_bounds = gfx::win::ScreenToDIPRect(
      window_->non_client_view()->GetWindowBoundsForClientBounds(dpi_bounds));
#else
  gfx::Rect window_bounds =
      window_->non_client_view()->GetWindowBoundsForClientBounds(bounds);
#endif
  // The window's position would also be changed, but we only want to change
  // the size.
  window_bounds.set_origin(origin);

  if (menu_bar_ && menu_bar_visible_)
    window_bounds.set_height(window_bounds.height() + kMenuBarHeight);
  return window_bounds;
}

ui::WindowShowState NativeWindowViews::GetRestoredState() {
  if (IsMaximized())
    return ui::SHOW_STATE_MAXIMIZED;
  if (IsFullscreen())
    return ui::SHOW_STATE_FULLSCREEN;

  return ui::SHOW_STATE_NORMAL;
}

// static
NativeWindow* NativeWindow::Create(
    brightray::InspectableWebContents* inspectable_web_contents,
    const mate::Dictionary& options) {
  return new NativeWindowViews(inspectable_web_contents, options);
}

}  // namespace atom
