"""Library presets for tools/gen_presets.py (spec 05 §4).

Each entry is (name, {param key: value}) over the Init patch (doc 04
defaults). Line parameters are given per line through L1/L2 dictionaries;
globals by their key. Slider values are 0..1 positions ("up" = more; for
times: up = longer). Quick reference for this engine (spec 02/03):

  lpf:  cutoff ~ 1.53 x 10*lpf x the 8' fundamental (key-tracked); 0.5 ~ 7.6 x f0
  hpf:  HPF sees 0.47 x (10*hpf + mods); 0.1 ~ 0.7 x f0, 0.3 ~ 2.2 x f0
  resH/resL: 0 = Q 0.5 (none), 0.5 ~ Q 1.6, 1 ~ Q 5 (less at high cutoff)
  il/al: filter EG start -5V*il, peak +5V*al around the cutoff slider
  VCA attack x: 2 ms * 442^x (0.5 = 42 ms, 0.7 = 141 ms, 0.8 = 260 ms); VCF attack to 580 ms
  VCA decay x: 2 ms * 3675^x (0.5 = 0.12 s, 0.7 = 0.63 s); VCF decay to 8.75 s
  release x: 2 ms * 5750^x (0.5 = 0.15 s, 0.7 = 0.85 s, 0.8 = 2.0 s); VCF release to 11 s
  env_long=1 [A]: all envelope times 2 ms .. 10 s (attack), 25 s (decay), 40 s (release)
  (ranges: Arturia CS-80 V manual; positions before 2026-10-09 were remapped to keep times)
  sub.speed: 0.5 Hz * 200^x (0.35 = 3.2 Hz, 0.45 = 5.4 Hz); sub.vco: 12 st * d^2
  pwmSpeed: 0.1 Hz * 1270^x;  rm.speed: 0.25 Hz + 204.75 Hz * x
  detune: 12 Hz * d^2 on line II;  porta.time: 1.4 ms * 1000^x per semitone
  feet: 0 16', 1 8', 2 5 1/3', 3 4', 4 2 2/3', 5 2'
"""

LIBRARY = []


def P(name, l1=None, l2=None, **glob):
    """Add a preset. l1/l2: line parameter dicts. Keys with '_' map to '.'."""
    vals = {}
    for prefix, d in (("l1", l1 or {}), ("l2", l2 or {})):
        for k, v in d.items():
            vals[f"{prefix}.{k}"] = v
    for k, v in glob.items():
        vals[k.replace("_", ".") if k not in ("bendRange",) else k] = v
    LIBRARY.append((name, vals))


def merge(*ds, **kw):
    out = {}
    for d in ds:
        out.update(d)
    out.update(kw)
    return out


# Shared line templates -------------------------------------------------------

SAW = dict(saw=1, square=0)
SQR = dict(saw=0, square=1, pw=0.0)
BOTH = dict(saw=1, square=1, pw=0.2)
SILENT = dict(level=0.0)

BRASS_LINE = merge(SAW, hpf=0.04, resH=0.0, lpf=0.34, resL=0.22, il=0.35, al=0.62,
                   fegA=0.731, fegD=0.785, fegR=0.628, vegA=0.362, vegD=0.701, vegS=0.92, vegR=0.601,
                   initBrill=0.45, initLevel=0.55, afterBrill=0.55, afterLevel=0.35, level=0.8)

STRING_LINE = merge(SAW, hpf=0.18, resH=0.15, lpf=0.62, resL=0.12, il=0.25, al=0.25,
                    fegA=0.828, fegD=0.851, fegR=0.748, vegA=0.703, vegD=0.785, vegS=0.95, vegR=0.76,
                    initBrill=0.25, initLevel=0.45, afterBrill=0.35, afterLevel=0.45, level=0.8)

PAD_LINE = merge(SAW, hpf=0.08, resH=0.0, lpf=0.42, resL=0.25, il=0.3, al=0.35,
                 fegA=0.913, fegD=0.892, fegR=0.829, vegA=0.793, vegD=0.869, vegS=1.0, vegR=0.824,
                 initBrill=0.2, initLevel=0.35, afterBrill=0.5, afterLevel=0.5, level=0.8)

PLUCK_LINE = merge(SAW, hpf=0.05, lpf=0.18, resL=0.35, il=0.0, al=0.8, fegA=0.0, fegD=0.563,
                   fegR=0.468, vegA=0.0, vegD=0.718, vegS=0.0, vegR=0.545, initBrill=0.5,
                   initLevel=0.7, afterBrill=0.2, afterLevel=0.2, level=0.85)

# Global "performance" templates.
VIB_TOUCH = dict(sub_func=0, sub_speed=0.457, touch_vco=0.28)       # aftertouch vibrato, CS-80 classic
CHORUS = dict(chorus=1, fx_speed=0.25, fx_depth=0.45)
HALL = dict(rev_mix=0.28, rev_decay=0.78, rev_tone=0.55, rev_predelay=0.3)
ROOM = dict(rev_mix=0.15, rev_decay=0.5, rev_tone=0.6, rev_predelay=0.1)
SPACE = dict(rev_mix=0.42, rev_decay=0.92, rev_tone=0.5, rev_predelay=0.45)

# --- IN: templates ------------------------------------------------------------

P("IN Single Line", l1=merge(SAW, lpf=0.6, resL=0.1), l2=SILENT, mix=0.5)
P("IN Two Lines Detuned", l1=merge(SAW), l2=merge(SAW), detune=0.3)

# --- BR: brass ------------------------------------------------------------------

P("BR Blade Runner Brass",
  l1=BRASS_LINE,
  l2=merge(BRASS_LINE, lpf=0.3, al=0.7, fegA=0.779),
  detune=0.32, brilliance=0.05, **VIB_TOUCH, **HALL)
P("BR End Titles Brass",
  l1=merge(BRASS_LINE, lpf=0.4, al=0.7, fegA=0.682, vegA=0.43),
  l2=merge(BRASS_LINE, feet=0, lpf=0.32, al=0.6, level=0.6),
  detune=0.25, touch_vco=0.32, sub_speed=0.414, **SPACE)
P("BR Toto Stab",
  l1=merge(BRASS_LINE, fegA=0.389, fegD=0.645, vegA=0.17, al=0.75, lpf=0.36, vegR=0.521),
  l2=merge(BRASS_LINE, square=1, saw=0, pw=0.3, fegA=0.426, fegD=0.645, vegA=0.17, lpf=0.32, vegR=0.521),
  detune=0.35, **ROOM)
P("BR Born In The USA",
  l1=merge(BRASS_LINE, lpf=0.42, al=0.55, fegA=0.487, fegD=0.728, vegA=0.226, resL=0.3),
  l2=merge(BRASS_LINE, feet=3, lpf=0.3, level=0.5, fegA=0.548, vegA=0.226),
  detune=0.28, **ROOM, **CHORUS)
P("BR French Horn",
  l1=merge(BRASS_LINE, lpf=0.22, resL=0.1, al=0.45, il=0.3, fegA=0.706, vegA=0.453, hpf=0.0, initBrill=0.35),
  l2=merge(BRASS_LINE, sine=0.45, vcfLevel=0.6, lpf=0.18, al=0.35, fegA=0.755, vegA=0.51),
  detune=0.15, **VIB_TOUCH, **HALL)
P("BR Swell Brass",
  l1=merge(BRASS_LINE, fegA=0.926, fegD=0.892, vegA=0.737, al=0.8, il=0.5),
  l2=merge(BRASS_LINE, fegA=0.974, fegD=0.917, vegA=0.771, al=0.85, il=0.5, lpf=0.28),
  detune=0.38, **VIB_TOUCH, **HALL)
P("BR Brass Section",
  l1=merge(BRASS_LINE, lpf=0.38),
  l2=merge(BRASS_LINE, lpf=0.36, fegA=0.706),
  detune=0.45, sub_speed=0.385, sub_vco=0.06, **CHORUS, **ROOM)
P("BR Wonder Brass",
  l1=merge(BRASS_LINE, square=1, pw=0.5, pwmDepth=0.4, pwmSpeed=0.348, lpf=0.4),
  l2=merge(BRASS_LINE, lpf=0.34),
  detune=0.3, **VIB_TOUCH, **ROOM)

# --- ST: strings ------------------------------------------------------------------

P("ST Vangelis Strings",
  l1=STRING_LINE, l2=merge(STRING_LINE, lpf=0.58, hpf=0.2),
  detune=0.42, sub_speed=0.414, sub_vco=0.07, **CHORUS, **HALL)
P("ST Billie Jean Strings",
  l1=merge(STRING_LINE, vegA=0.396, fegA=0.487, hpf=0.24, lpf=0.66, vegR=0.625),
  l2=merge(STRING_LINE, vegA=0.396, fegA=0.487, hpf=0.24, lpf=0.62, vegR=0.625),
  detune=0.38, **CHORUS, **ROOM)
P("ST Thin Bowed",
  l1=merge(STRING_LINE, hpf=0.3, resH=0.4, lpf=0.75, resL=0.3, vegA=0.623),
  l2=merge(STRING_LINE, square=1, saw=0, pw=0.5, pwmDepth=0.35, pwmSpeed=0.386, hpf=0.28, lpf=0.7),
  detune=0.3, touch_vco=0.2, sub_speed=0.442, **CHORUS, **HALL)
P("ST Cello Section",
  l1=merge(STRING_LINE, feet=0, hpf=0.1, lpf=0.5, resL=0.25, vegA=0.567),
  l2=merge(STRING_LINE, hpf=0.12, lpf=0.46, vegA=0.589, level=0.65),
  detune=0.3, **VIB_TOUCH, **HALL)
P("ST Octave Ensemble",
  l1=STRING_LINE, l2=merge(STRING_LINE, feet=3, hpf=0.3, lpf=0.6, level=0.6),
  detune=0.25, **CHORUS, **HALL)
P("ST Pizzicato",
  l1=merge(STRING_LINE, vegA=0.0, vegD=0.549, vegS=0.0, vegR=0.505, fegA=0.0, fegD=0.505, al=0.6, il=0.0),
  l2=merge(STRING_LINE, vegA=0.0, vegD=0.575, vegS=0.0, vegR=0.521, fegA=0.0, fegD=0.522, al=0.55, il=0.0),
  detune=0.25, **ROOM)

# --- PD: pads --------------------------------------------------------------------------

P("PD Blade Pad",
  l1=PAD_LINE, l2=merge(PAD_LINE, square=1, saw=0, pw=0.4, pwmDepth=0.5, pwmSpeed=0.27, lpf=0.38),
  detune=0.4, **VIB_TOUCH, **CHORUS, **SPACE)
# Long envelope mode [A]: slow filter swell (VCA ~1.2 s, VCF ~1.8 s attack) and ~6 s tails.
P("PD Long Blade Swell",
  l1=merge(PAD_LINE, lpf=0.32, resL=0.3, il=0.4, al=0.55, fegA=0.8, fegD=0.85, fegR=0.8,
           vegA=0.75, vegD=0.8, vegR=0.8),
  l2=merge(PAD_LINE, square=1, saw=0, pw=0.4, pwmDepth=0.5, pwmSpeed=0.27, lpf=0.28, al=0.5,
           fegA=0.82, fegD=0.88, fegR=0.82, vegA=0.77, vegD=0.8, vegR=0.82),
  env_long=1, detune=0.4, **VIB_TOUCH, **CHORUS, **SPACE)
P("PD Memories Of Green",
  l1=merge(PAD_LINE, saw=0, square=1, pw=0.0, lpf=0.3, sine=0.4, vcfLevel=0.7),
  l2=merge(PAD_LINE, saw=0, sine=0.7, vcfLevel=0.0, feet=3),
  porta_time=0.38, detune=0.2, touch_vco=0.3, sub_speed=0.414, **SPACE)
P("PD Tears In Rain",
  l1=merge(PAD_LINE, lpf=0.36, resL=0.4, il=0.6, al=0.5, fegA=0.974),
  l2=merge(PAD_LINE, feet=0, lpf=0.3, level=0.55),
  detune=0.45, sub_speed=0.299, sub_vcf=0.08, **CHORUS, **SPACE)
P("PD Spiral Haze",
  l1=merge(PAD_LINE, square=1, saw=0, pw=0.5, pwmDepth=0.6, pwmSpeed=0.309, lpf=0.45),
  l2=merge(PAD_LINE, feet=3, lpf=0.4, hpf=0.25, level=0.6),
  detune=0.35, sub_func=0, sub_speed=0.127, sub_vcf=0.12, **CHORUS, **SPACE)
P("PD Love Theme",
  l1=merge(PAD_LINE, sine=0.6, vcfLevel=0.5, lpf=0.35),
  l2=merge(PAD_LINE, lpf=0.3, resL=0.1),
  detune=0.3, **VIB_TOUCH, **SPACE)
P("PD Dark Cathedral",
  l1=merge(PAD_LINE, feet=0, lpf=0.3, resL=0.45, il=0.5, al=0.3),
  l2=merge(PAD_LINE, lpf=0.26, resL=0.4, level=0.7),
  detune=0.5, sub_speed=0.055, sub_vcf=0.1, brilliance=-0.1, **SPACE)
P("PD Warm Analog",
  l1=PAD_LINE, l2=merge(PAD_LINE, lpf=0.4),
  detune=0.3, **CHORUS, **HALL)
P("PD Aftertouch Swell",
  l1=merge(PAD_LINE, lpf=0.2, afterBrill=1.0, afterLevel=0.9, initLevel=0.2),
  l2=merge(PAD_LINE, lpf=0.18, afterBrill=0.9, afterLevel=0.9, initLevel=0.2),
  detune=0.35, touch_vco=0.25, touch_vcf=0.1, sub_speed=0.414, **SPACE)
P("PD Glass Pad",
  l1=merge(PAD_LINE, hpf=0.35, resH=0.55, lpf=0.75, resL=0.35),
  l2=merge(PAD_LINE, sine=0.6, vcfLevel=0.3, feet=3),
  detune=0.3, **CHORUS, **SPACE)
P("PD Noise Breath",
  l1=merge(PAD_LINE, noise=0.35, lpf=0.4, resL=0.5),
  l2=merge(PAD_LINE, lpf=0.32),
  detune=0.3, sub_speed=0.198, sub_vcf=0.1, **SPACE)

# --- LD: leads ---------------------------------------------------------------------------------

LEAD_LINE = merge(SAW, hpf=0.05, lpf=0.42, resL=0.4, il=0.2, al=0.5, fegA=0.426, fegD=0.686, fegR=0.588,
                  vegA=0.226, vegD=0.701, vegS=0.9, vegR=0.545, initBrill=0.4, initLevel=0.6,
                  afterBrill=0.5, afterLevel=0.3, level=0.85)

P("LD Chariots Lead",
  l1=LEAD_LINE, l2=merge(LEAD_LINE, feet=3, lpf=0.32, level=0.5),
  detune=0.2, porta_time=0.3, **VIB_TOUCH, **HALL)
P("LD Ribbon Solo",
  l1=merge(LEAD_LINE, lpf=0.5), l2=merge(LEAD_LINE, square=1, saw=0, pw=0.3, lpf=0.45),
  detune=0.25, porta_time=0.25, touch_vco=0.35, sub_speed=0.485, bendRange=12, **HALL)
P("LD Sine Whistle",
  l1=merge(LEAD_LINE, vcfLevel=0.0, sine=1.0, vegA=0.34), l2=merge(LEAD_LINE, vcfLevel=0.0, sine=0.6, feet=3, level=0.5),
  detune=0.15, porta_time=0.32, **VIB_TOUCH, **HALL)
P("LD Sync-Free Scream",
  l1=merge(LEAD_LINE, lpf=0.3, resL=0.85, al=0.9, fegA=0.243, fegD=0.645),
  l2=merge(LEAD_LINE, feet=3, lpf=0.3, resL=0.8, al=0.85, level=0.6),
  detune=0.3, **VIB_TOUCH, **ROOM)
P("LD Square Lead",
  l1=merge(LEAD_LINE, saw=0, square=1, pw=0.25, lpf=0.5),
  l2=SILENT, porta_time=0.2, **VIB_TOUCH, **ROOM)
P("LD Fifth Lead",
  l1=LEAD_LINE, l2=merge(LEAD_LINE, feet=2, level=0.65),
  porta_time=0.22, **VIB_TOUCH, **ROOM)
P("LD Glide Pulse",
  l1=merge(LEAD_LINE, saw=0, square=1, pw=0.5, pwmDepth=0.45, pwmSpeed=0.386),
  l2=merge(LEAD_LINE, feet=0, level=0.6), porta_time=0.42, detune=0.2, **ROOM)
P("LD Glissando Steps",
  l1=LEAD_LINE, l2=merge(LEAD_LINE, feet=3, level=0.5), porta_mode=1, porta_time=0.35, **HALL)

# --- BS: bass ------------------------------------------------------------------------------------

BASS_LINE = merge(SAW, feet=0, hpf=0.0, lpf=0.22, resL=0.35, il=0.0, al=0.6, fegA=0.0, fegD=0.563,
                  fegR=0.427, vegA=0.0, vegD=0.701, vegS=0.75, vegR=0.425, initBrill=0.5, initLevel=0.6,
                  afterBrill=0.3, afterLevel=0.2, level=0.9)

P("BS Analog Bass", l1=BASS_LINE, l2=merge(BASS_LINE, square=1, saw=0, pw=0.2, lpf=0.18, level=0.7), detune=0.15)
P("BS Christmastime Stab",
  l1=merge(BASS_LINE, feet=1, lpf=0.15, resL=0.5, al=0.7),
  l2=merge(BASS_LINE, feet=1, square=1, saw=0, lpf=0.15),
  sub_func=2, sub_speed=0.299, sub_vcf=0.45, detune=0.3, **ROOM)
P("BS Rubber Bass", l1=merge(BASS_LINE, resL=0.7, al=0.75, fegD=0.505), l2=SILENT, porta_time=0.15)
P("BS Deep Sub", l1=merge(BASS_LINE, sine=0.8, vcfLevel=0.4, lpf=0.15), l2=merge(BASS_LINE, square=1, saw=0, feet=0, lpf=0.12, level=0.5))
P("BS Funky Bass",
  l1=merge(BASS_LINE, square=1, saw=0, pw=0.4, lpf=0.12, resL=0.55, al=0.85, fegD=0.48, vegS=0.4),
  l2=SILENT, **ROOM)
P("BS Brass Bass", l1=merge(BRASS_LINE, feet=0, fegA=0.426, vegA=0.113), l2=merge(BASS_LINE, lpf=0.2), detune=0.2)

# --- KY: keys ------------------------------------------------------------------------------------

P("KY CS Electric Piano",
  l1=merge(PLUCK_LINE, saw=0, square=1, pw=0.57, lpf=0.3, al=0.6, vegD=0.802, sine=0.5),
  l2=merge(PLUCK_LINE, saw=0, sine=0.8, vcfLevel=0.0, feet=3, vegD=0.718, level=0.5),
  detune=0.2, tremolo=1, fx_speed=0.35, fx_depth=0.25, **ROOM)
P("KY Clavichord",
  l1=merge(PLUCK_LINE, saw=1, square=1, pw=0.0, lpf=0.0, al=1.0, fegD=0.645, afterBrill=0.85, vegD=0.684),
  l2=SILENT, **ROOM)
P("KY Harpsichord",
  l1=merge(PLUCK_LINE, saw=0, square=1, pw=0.57, hpf=0.35, resH=0.0, lpf=0.85, resL=0.0, al=0.0, vegD=0.785, vegR=0.385),
  l2=merge(PLUCK_LINE, saw=0, square=1, pw=0.57, feet=3, hpf=0.4, lpf=0.85, al=0.0, vegD=0.751, vegR=0.385, level=0.6),
  **ROOM)
P("KY Funky Clav",
  l1=merge(PLUCK_LINE, saw=0, square=1, pw=0.3, lpf=0.15, resL=0.6, al=0.9, fegD=0.456, vegD=0.617),
  l2=SILENT, **ROOM)
P("KY Toy Piano",
  l1=merge(PLUCK_LINE, saw=0, sine=0.9, vcfLevel=0.3, feet=3, vegD=0.659),
  l2=merge(PLUCK_LINE, saw=0, sine=0.6, vcfLevel=0.0, feet=5, vegD=0.575, level=0.5), **HALL)
P("KY Polysynth Stab",
  l1=merge(BRASS_LINE, fegA=0.0, fegD=0.604, vegA=0.0, vegD=0.659, vegS=0.3, al=0.6, il=0.0, lpf=0.3),
  l2=merge(BRASS_LINE, fegA=0.0, fegD=0.62, vegA=0.0, vegD=0.659, vegS=0.3, al=0.6, il=0.0, lpf=0.28),
  detune=0.35, **CHORUS, **ROOM)

# --- OR: organ -------------------------------------------------------------------------------------

ORGAN_LINE = merge(SQR, hpf=0.05, lpf=0.62, resL=0.0, il=0.0, al=0.0, fegA=0.0, vegA=0.0, vegD=0.617,
                   vegS=1.0, vegR=0.385, initBrill=0.1, initLevel=0.2, afterBrill=0.2, afterLevel=0.2,
                   sine=0.4, level=0.75)

P("OR Combo Organ", l1=ORGAN_LINE, l2=merge(ORGAN_LINE, feet=3, level=0.55), tremolo=1, fx_speed=0.55, fx_depth=0.3)
P("OR Church Organ",
  l1=merge(ORGAN_LINE, feet=0, saw=1, square=0, lpf=0.45, vegA=0.283, vegR=0.585),
  l2=merge(ORGAN_LINE, feet=3, lpf=0.5, vegA=0.283, vegR=0.585, level=0.6), **SPACE)
P("OR Sine Drawbars",
  l1=merge(ORGAN_LINE, vcfLevel=0.0, sine=1.0), l2=merge(ORGAN_LINE, vcfLevel=0.0, sine=0.8, feet=4),
  chorus=1, fx_speed=0.6, fx_depth=0.35, **ROOM)
P("OR Reed Organ",
  l1=merge(ORGAN_LINE, saw=1, square=0, hpf=0.25, resH=0.3, lpf=0.5), l2=merge(ORGAN_LINE, pw=0.4, level=0.6),
  detune=0.2, **ROOM)

# --- PL: pluck / mallet --------------------------------------------------------------------------

P("PL Guitar Pluck",
  l1=merge(PLUCK_LINE, lpf=0.25, resL=0.3, al=0.75, fegD=0.538, vegD=0.751),
  l2=merge(PLUCK_LINE, saw=0, square=1, pw=0.43, lpf=0.22, fegD=0.522, vegD=0.718, level=0.6),
  detune=0.25, **CHORUS, **ROOM)
P("PL Marimba",
  l1=merge(PLUCK_LINE, saw=0, sine=1.0, vcfLevel=0.25, vegD=0.575, vegR=0.465),
  l2=merge(PLUCK_LINE, saw=0, square=1, feet=3, lpf=0.15, vegD=0.448, level=0.35), **ROOM)
P("PL Koto",
  l1=merge(PLUCK_LINE, saw=0, square=1, pw=0.7, hpf=0.2, lpf=0.35, resL=0.5, fegD=0.522, vegD=0.718),
  l2=SILENT, **HALL)
P("PL Harp",
  l1=merge(PLUCK_LINE, lpf=0.3, al=0.5, vegD=0.827, vegR=0.705, fegD=0.645),
  l2=merge(PLUCK_LINE, sine=0.6, vcfLevel=0.4, feet=3, vegD=0.785, vegR=0.665, level=0.5),
  detune=0.15, **HALL)
P("PL Pluck Echo",
  l1=merge(PLUCK_LINE, lpf=0.2, resL=0.55, fegD=0.563),
  l2=merge(PLUCK_LINE, feet=3, lpf=0.2, resL=0.5, level=0.5), detune=0.3, **CHORUS, **SPACE)

# --- BL: bells / metallic --------------------------------------------------------------------------

BELL_LINE = merge(PLUCK_LINE, saw=0, sine=0.9, vcfLevel=0.2, vegA=0.0, vegD=0.869, vegS=0.0, vegR=0.784)

P("BL Ring Mod Bells", l1=BELL_LINE, l2=merge(BELL_LINE, feet=4, level=0.6),
  rm_mod=0.75, rm_speed=0.55, rm_depth=0.0, **SPACE)
P("BL Tubular Bells", l1=merge(BELL_LINE, square=1, pw=0.6, vcfLevel=0.4, lpf=0.4),
  l2=merge(BELL_LINE, feet=5, level=0.45), rm_mod=0.45, rm_speed=0.85, **HALL)
P("BL Glass Chimes", l1=merge(BELL_LINE, feet=3), l2=merge(BELL_LINE, feet=4, level=0.7),
  detune=0.4, **CHORUS, **SPACE)
P("BL Metal Sweep", l1=merge(BRASS_LINE, vegR=0.744), l2=merge(BRASS_LINE, feet=4, level=0.6),
  rm_mod=0.6, rm_speed=0.15, rm_depth=0.8, rm_attack=0.589, rm_decay=0.91, **HALL)

# --- SQ: sequence friendly -------------------------------------------------------------------------

P("SQ Arp Pluck", l1=merge(PLUCK_LINE, lpf=0.22, resL=0.45, fegD=0.48, vegD=0.575), l2=SILENT, **ROOM)
P("SQ Pulse Arp", l1=merge(PLUCK_LINE, saw=0, square=1, pw=0.5, pwmDepth=0.4, pwmSpeed=0.425, lpf=0.3),
  l2=merge(PLUCK_LINE, feet=0, lpf=0.15, level=0.6), detune=0.2, **CHORUS)
P("SQ Sample & Hold Blips", l1=merge(PLUCK_LINE, lpf=0.3, resL=0.7, fegD=0.439), l2=SILENT,
  sub_func=4, sub_speed=0.385, sub_vcf=0.5, **HALL)

# --- FX: effects / sci-fi --------------------------------------------------------------------------

P("FX Spinner Flyby",
  l1=merge(PAD_LINE, noise=0.6, saw=0, lpf=0.3, resL=0.8), l2=merge(PAD_LINE, feet=0, lpf=0.2),
  sub_func=0, sub_speed=0.0, sub_vcf=0.6, sub_vco=0.25, **SPACE)
P("FX Wind", l1=merge(PAD_LINE, saw=0, noise=1.0, lpf=0.35, resL=0.7, hpf=0.1), l2=SILENT,
  sub_func=5, sub_speed=0.0, sub_vcf=0.6, **SPACE)
P("FX Laser Siren", l1=merge(LEAD_LINE, lpf=0.5), l2=merge(LEAD_LINE, feet=3, level=0.5),
  sub_func=1, sub_speed=0.241, sub_vco=0.6, **HALL)
P("FX Robot Burble", l1=merge(LEAD_LINE, resL=0.8, lpf=0.25), l2=SILENT,
  sub_func=4, sub_speed=0.586, sub_vcf=0.6, rm_mod=0.4, rm_speed=0.4, **ROOM)
P("FX Audio Rate FM", l1=merge(LEAD_LINE, lpf=0.6), l2=SILENT,
  sub_func=0, sub_speed=0.873, sub_vco=0.35, **ROOM)
P("FX Space Drone", l1=merge(PAD_LINE, feet=0, resL=0.6, lpf=0.25), l2=merge(PAD_LINE, feet=2, lpf=0.3),
  detune=0.6, sub_speed=0.0, sub_vcf=0.25, rm_mod=0.25, rm_speed=0.05, **SPACE)
