# maxisnare: logue SDK 2.0 Synth unit using for a (much louder/noisier) snare

This forks boochow' excellent work from maxisynth

Discord complained about the korg drumlogue snare not being loud enough.

Search no more.

It can also take care of your gabber kick, trap 808, dnb reese, and a bunch of
noisy cymbals too

## Controls

### Synth snare (page 1)

- `Decay` = Length of a hit
- `Wave` = choose a sine, triangle, square, or saw
- `Osc/Nse` = balance the oscillator versus the white noise

### Wave shaping (page 2)

Implement a pitch envelope, and the famous poutoushaper

- `PitchDecay` = size of the pitch envelope
- `>Pitch` = ...its intensity on the oscillator
- `Shape` = pick a saturating wave shaper (among softclipping, hardclipping, and atan function)
- `Gain` = The gain before entering the poutoushaper

### Final EQ (page 3)

This happens AFTER the clipping stage, and can drive the drumlogue internal engine.

> ie. : This can be _LOUD_, and ringy

You can pick low/high shelving EQ or a pick, gain can go up to +30dB for a really loud
bump, negative and smaller value let you shape your sound more cleanly

- `Gain` = -15db to +30dB at the cutoff frequency (after the waveshaping)
- `Cutoff` = self exp.
- `Resonance` = it won't self oscillate, but with steep gain, it'll be quite ringy
- `Shape` = choose among LowShelve, HighShelve, and Peak

## How to build

(words from `boochow`)

Since Maximilian currently has some minor issues to use with logue SDK, use the patched fork of Maximilian, which I am providing at:

https://github.com/boochow/Maximilian

Place these repositories under `logue-sdk/platforms/drumlogue/` like this:

```
drumlogue/
├── common
├── Maximilian
└── maxisnare
```

then type

`../../docker/run_cmd.sh build drumlogue/maxisnare`

and copy `maxisnare/maxisnare.drmlgunit` to your drumlogue.

## Upstream project

<https://github.com/boochow/maxisynth>
