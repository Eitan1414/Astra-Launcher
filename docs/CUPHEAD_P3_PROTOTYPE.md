# Cuphead Wii U — Player 3 Prototype

This branch is the first real gameplay prototype for a third local Cuphead player.

The goal is deliberately small:

```text
Player 1 works
Player 2 works
Player 3 spawns
Player 3 has an independent controller
Player 3 can move / jump / shoot
```

HUD polish, revive behavior, camera tuning, special bosses and Player 4 come later.

## Verified Wii U input architecture

The current Wii U `Assembly-CSharp.dll` was inspected directly before designing the prototype.

### Input controller storage

The original game initializes:

```text
Input.playerControllers = new ControllerTypes[2]
Input.disconnected      = new bool[2]
```

Controller type values are:

```text
Gamepad             = 0
ProController       = 1
Wiimote             = 2
WiimoteAndNunchuk   = 3
ClassicController   = 4
None                = 5
```

### Physical Wii U controller sources

The original `Input.Start()` already opens:

```text
GamePad
Remote channel 0
Remote channel 1
```

and `Input.Update()` stores:

```text
gamepadState
remoteState   // channel 0
remoteState2  // channel 1
```

This means the first P3 prototype does **not** need a third Wii Remote channel.

For the MVP we can use:

```text
P1 = Wii U GamePad
P2 = Remote channel 0
P3 = Remote channel 1
```

A later four-player implementation will need another physical source, most likely `Remote.Access(2)`, plus a third remote state.

## Required managed changes for P3

### PlayerId

Add:

```text
PlayerThree = 2
```

while preserving:

```text
PlayerOne = 0
PlayerTwo = 1
Any       = 2147483646
None      = 2147483647
```

### Input

Expand:

```text
playerControllers: 2 -> 3
disconnected:      2 -> 3
```

Add PlayerThree recognition to:

```text
GetButtonUp
GetButton
GetButtonDown
GetAxis
```

The original methods identify the player by searching the input action string for `PlayerId.ToString()`. The P3 version must also recognize `PlayerThree`, select player index 2 and route its non-GamePad input through `remoteState2`.

Add a P3 controller-selection path equivalent to `P2_GetCurrentController`, but restricted to the remaining controller source for the first MVP.

### PlayerManager

Expand:

```text
playerSlots:       2 -> 3
validIDs:          [P1, P2] -> [P1, P2, P3]
playerWasChalice:  2 -> 3
players dictionary -> add PlayerThree
```

Add:

```text
Multiplayer2
```

for Player 3 state.

The original `PlayerManager.Update()` contains several two-player assumptions that must be replaced:

```text
slot 0 -> PlayerOne
all non-zero slots -> PlayerTwo
```

becomes:

```text
slot 0 -> PlayerOne
slot 1 -> PlayerTwo
slot 2 -> PlayerThree
```

The same correction is needed for join, leave, disconnect and controller assignment handling.

Online user switching can remain limited to P1/P2 for the first offline P3 prototype.

### Level

Expand:

```text
players:            2 -> 3
BlockChaliceCharm:  2 -> 3
```

Add P3 creation when `PlayerManager.Multiplayer2` is active.

The existing `Level.OnPlayerJoined()` always calls `CreatePlayerTwoOnJoin()`, so the P3 prototype needs either:

- a new `CreatePlayerThreeOnJoin()` call from `OnPlayerJoined()`, or
- equivalent P3 creation logic added to the existing join path.

The PC four-player demo uses a dedicated `CreatePlayerThreeOnJoin()`; that is the preferred structure for the Wii U port as well.

## Initial controller restriction

The first prototype intentionally uses:

```text
P1 = GamePad
P2 = first external controller
P3 = second external controller
```

This keeps the first test focused on proving three independent players without immediately rewriting the entire Wii U controller allocator.

Once P3 is stable we can generalize controller selection.

## Astra package layout

The prototype package source folder is:

```text
examples/Cuphead3PlayerPrototype/
├── mod.json
└── content/
    └── Managed/
        └── Assembly-CSharp.dll   <- generated locally, never committed
```

The modified proprietary Cuphead DLL must not be committed to this repository.

The PC-side Astra Packager turns that folder into:

```text
Cuphead3PlayerPrototype.sol
```

Astra v0.6 can already validate/decrypt the SOL index and now has an on-demand API for decrypting and zlib-decompressing individual package files.

The remaining Astra-side step is connecting that in-memory file data to the redirection layer used by the game.

## Prototype success criteria

The first successful test is reached when all of the following are true:

- Cuphead launches through Astra with the original installed game untouched.
- Astra loads the modified `Managed/Assembly-CSharp.dll` from the enabled SOL package.
- P1 and P2 still behave normally.
- P3 can join/spawn in a normal level.
- P3 reads a different physical controller from P1 and P2.
- P3 can move, jump and shoot.
- Leaving/restarting a level does not immediately crash.

Everything after that is iteration rather than proof-of-concept.
