//! Input and controller vtable hooks,
//! and the menu listener.
//!
//! Part of `Plugin.cpp`: included
//! inside its anonymous namespace, in
//! order, and relies on the fragments
//! before it. Do not include it
//! anywhere else.



/// Hides vanilla input handlers' events
/// that a climbing binding owns.
template <class Handler, int Index> struct InputGuard {
  static inline REL::Relocation<bool (*)(Handler *, RE::InputEvent *)> original;
  /// Hooked `CanProcess`.
  static bool canProcess(Handler *self, RE::InputEvent *event) {
    if (traversal.active()) {

      if (event)
        if (auto button = event->AsButtonEvent(); button && button->IsUp())
          return original(self, event);
      return false;
    }
    return original(self, event);
  }
  /// Hook the handler vtable.
  static void install(REL::VariantID table) {
    REL::Relocation<std::uintptr_t> vtable{table};
    original = vtable.write_vfunc(fc::runtime::inputFilterSlot, canProcess);
  }
};

/// Keyboard device hook: samples keys
/// and removes owned key events before
/// the game sees them.
struct AttachedSpaceGuard {
  static inline REL::Relocation<void (*)(RE::BSWin32KeyboardDevice *, float)>
      original;
  /// Hooked keyboard `Process`.
  static void process(RE::BSWin32KeyboardDevice *keyboard, float dt) {
    original(keyboard, dt);
    fc::settingsMenuKeyboardSample(keyboard->GetRuntimeData().curState);
    const bool suspended = grabInputSuspended();
    if (suspended) {
      cancelGrabRequest();
      inputState.reset();
      wallRunEntryGate.reset();
    }
    auto *queue = RE::BSInputEventQueue::GetSingleton();
    if (!queue)
      return;
    entryLookDiagnostics.queue(queue->GetQueueHead(), 0);
    std::array<std::uint32_t, 4> nativeMovementKeys{0xffffffffu, 0xffffffffu,
                                                    0xffffffffu, 0xffffffffu};
    if (const auto *controls = RE::ControlMap::GetSingleton();
        !suspended && controls && controls->IsMovementControlsEnabled()) {
      const std::array<std::string_view, 4> events{
          "Forward", "Back", "Strafe Left", "Strafe Right"};
      for (std::size_t i = 0; i < events.size(); ++i)
        nativeMovementKeys[i] =
            controls->GetMappedKey(events[i], RE::INPUT_DEVICE::kKeyboard);
    }
    fc::removeInputEvents(
        queue->GetQueueHead(), queue->GetQueueTail(),
        [suspended, nativeMovementKeys](RE::InputEvent *event) {
          if (fc::settingsMenuFilterInput(event))
            return true;
          auto *button = event->AsButtonEvent();
          if (!button || event->GetDevice() != RE::INPUT_DEVICE::kKeyboard)
            return false;
          const auto scan = button->GetIDCode();
          const auto before = inputState;
          if (!suspended)
            inputState.set(scan, button->IsPressed());
          if (button->IsDown() && !suspended && !traversal.active())
            gamepadPreferred = false;
          if (traversal.active() && !gamepadOwned)
            wallRunEntryGate.filter(
                fc::mapKeys(inputState, activeSettings.bindings));
          return inputOwnership.filter(
              scan, button->IsDown(), button->IsUp(),
              !suspended && traversal.active() && !gamepadOwned, before,
              inputState, activeSettings.bindings,
              !suspended && std::find(nativeMovementKeys.begin(),
                                      nativeMovementKeys.end(),
                                      scan) != nativeMovementKeys.end());
        });
    entryLookDiagnostics.queue(queue->GetQueueHead(), 1);
  }
  /// Hook the keyboard device.
  static void install() {
    REL::Relocation<std::uintptr_t> table{RE::VTABLE_BSWin32KeyboardDevice[0]};
    original = table.write_vfunc(fc::runtime::keyboardProcessSlot, process);
  }
};

/// Gamepad device hook: samples XInput
/// and removes owned button events.
struct GamepadGuard {
  static inline REL::Relocation<void (*)(RE::BSPCGamepadDeviceHandler *, float)>
      original;
  /// The XInput device of a handler.
  static RE::BSWin32GamepadDevice *
  device(RE::BSPCGamepadDeviceHandler *handler) {
    auto *delegate =
        handler ? handler->GetRuntimeData().currentPCGamePadDelegate : nullptr;
    return delegate && delegate->IsEnabled()
               ? skyrim_cast<RE::BSWin32GamepadDevice *>(delegate)
               : nullptr;
  }
  /// Feed the gamepad state to input
  /// tracking and the menu.
  static void sample(const RE::BSWin32GamepadDevice &pad) {
    const auto &raw = pad.GetRuntimeData().currentState.gamepad;
    gamepadState.sampleXInput(raw.buttons, raw.leftTrigger, raw.rightTrigger,
                              raw.thumbLX, raw.thumbLY, activeSettings.gamepad);
  }
  /// Hooked gamepad `Process`.
  static void process(RE::BSPCGamepadDeviceHandler *handler, float dt) {
    original(handler, dt);
    auto *pad = device(handler);
    const bool available = pad != nullptr;
    if (available != gamepadAvailable) {
      SKSE::log::info("XInput controller available={}", available);
      gamepadLost |= gamepadOwned && !available;
      gamepadState.reset();
      gamepadOwnership.reset();
      gamepadPreferred = false;
      gamepadAvailable = available;
    }
    if (!pad) {
      fc::settingsMenuGamepadSample(false);
      return;
    }
    const auto &raw = pad->GetRuntimeData().currentState.gamepad;
    fc::settingsMenuGamepadSample(true, raw.buttons, raw.leftTrigger,
                                  raw.rightTrigger);
    const bool suspended =
        grabInputSuspended() || !enabled || !activeSettings.gamepad.enabled;
    if (suspended)
      gamepadState.blockUntilButtonsReleased();
    const auto before = gamepadState;
    sample(*pad);
    auto *queue = RE::BSInputEventQueue::GetSingleton();
    if (!queue)
      return;
    fc::removeInputEvents(
        queue->GetQueueHead(), queue->GetQueueTail(),
        [suspended, &before](RE::InputEvent *event) {
          if (fc::settingsMenuFilterInput(event))
            return true;
          if (event->GetDevice() != RE::INPUT_DEVICE::kGamepad)
            return false;
          if (auto *button = event->AsButtonEvent()) {
            const auto index = gamepadButtonIndex(button->GetIDCode());
            if (index >= 16)
              return false;
            if (!suspended && !traversal.active() && button->IsDown())
              gamepadPreferred = true;
            return gamepadOwnership.filter(
                index, button->IsDown(), button->IsUp(),
                !suspended && traversal.active() && gamepadOwned, before,
                gamepadState, activeSettings.gamepad.bindings);
          }
          if (const auto *stick = event->AsThumbstickEvent();
              stick && stick->IsLeft() && !suspended && !traversal.active() &&
              std::hypot(stick->xValue, stick->yValue) >
                  activeSettings.gamepad.deadzone)
            gamepadPreferred = true;
          return false;
        });
    if (!suspended)
      gamepadState.resumeIfButtonsReleased();
    if (traversal.active() && gamepadOwned)
      wallRunEntryGate.filter(
          gamepadState.keys(activeSettings.gamepad.bindings));
  }
  /// Hook the gamepad device.
  static void install() {
    REL::Relocation<std::uintptr_t> table{
        RE::VTABLE_BSPCGamepadDeviceHandler[0]};
    if (!fc::runtime::hookSite(table.address(), fc::runtime::gamepadPollSlot)) {
      SKSE::log::error(
          "Controller input hook unavailable; keyboard controls retained");
      return;
    }
    original = table.write_vfunc(fc::runtime::gamepadPollSlot, process);
    gamepadHookReady = true;
    gamepadState.reset();
  }
};

/// Releases the climb when a menu that
/// pauses the game opens.
struct MenuListener : RE::BSTEventSink<RE::MenuOpenCloseEvent> {
  /// Menu open/close callback.
  RE::BSEventNotifyControl
  ProcessEvent(const RE::MenuOpenCloseEvent *e,
               RE::BSTEventSource<RE::MenuOpenCloseEvent> *) override {
    if (e && e->opening && e->menuName != "HUD Menu" &&
        e->menuName != "Cursor Menu") {
      traversalAudio.stop();

      inputState.reset();
      gamepadState.blockUntilButtonsReleased();
      wallRunEntryGate.reset();
      if (traversal.active())
        release(RE::PlayerCharacter::GetSingleton(), "menu opened");
      cancelGrabRequest();
    }
    return RE::BSEventNotifyControl::kContinue;
  }
} menuListener;

/// Whether every hook site is valid on
/// this runtime.
bool runtimeHooksReady() {
  if (!fc::runtime::supported())
    return false;
  const auto check = [](REL::VariantID id, std::size_t slot) {
    REL::Relocation<std::uintptr_t> table{id};
    if (fc::runtime::hookSite(table.address(), slot))
      return true;
    SKSE::log::error("Runtime hook target unavailable: table={:X}, slot={:X}; "
                     "hooks not installed",
                     table.address(), slot);
    return false;
  };
  if (!check(RE::VTABLE_PlayerCharacter[0], fc::runtime::actorUpdateSlot) ||
      !check(RE::VTABLE_BSWin32KeyboardDevice[0],
             fc::runtime::keyboardProcessSlot))
    return false;
  for (const auto id :
       {RE::VTABLE_MovementHandler[0], RE::VTABLE_JumpHandler[0],
        RE::VTABLE_SneakHandler[0], RE::VTABLE_SprintHandler[0],
        RE::VTABLE_ReadyWeaponHandler[0], RE::VTABLE_AttackBlockHandler[0],
        RE::VTABLE_ActivateHandler[0], RE::VTABLE_TogglePOVHandler[0],
        RE::VTABLE_ShoutHandler[0], RE::VTABLE_AutoMoveHandler[0],
        RE::VTABLE_RunHandler[0], RE::VTABLE_ToggleRunHandler[0]})
    if (!check(id, fc::runtime::inputFilterSlot))
      return false;
  return true;
}
