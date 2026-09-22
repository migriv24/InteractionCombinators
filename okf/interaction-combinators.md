---
type: Concept
title: The mathematics of interaction combinators
description: "The mini-OKF of the math itself — Lafont's three agents, the six rules, the properties (locality, strong confluence, universality), vicious circles, and how each maths object maps onto this demo's model. The reference the app is checked against."
tags: [status:current, audience:all, confidence:asserted]
timestamp: 2026-07-13T00:00:00Z
---

Requested by the author (2026-07-13): a mini-OKF of *the mathematical concept
itself*, separate from the app — so that "is the app right?" always has a
ground truth to check against, and so later work (rule editing, new demos,
teaching material) has the map. Source: Lafont, *Interaction Combinators*
(Information & Computation 137, 1997), building on *Interaction Nets* (POPL
1990).

# Agents, ports, nets

An **agent** is a cell with a **symbol** and a fixed **arity** — the number of
**auxiliary ports** it carries in addition to exactly one **principal port**.
The interaction combinators are just three symbols:

| symbol | name | arity | drawn as |
|---|---|---|---|
| γ (gamma) | constructor | 2 | triangle, principal at the apex |
| δ (delta) | duplicator | 2 | triangle, principal at the apex |
| ε (epsilon) | eraser | 0 | small circle |

A **net** is a graph of agents where **every port holds exactly one wire
end**. Wires are undirected; a wire may join *any* two ports — principal to
principal, principal to auxiliary, auxiliary to auxiliary — or even both ends
of itself (a free-standing loop). Ports not wired to anything are the net's
**free ports**, its interface. There is no direction and no dataflow in the
mathematics — direction is a reading a host imposes, not a property of nets.

# Active pairs and the six rules

Two agents whose **principal ports** are wired together form an **active
pair** (a **redex**) — the only configuration that may rewrite. With three
symbols there are exactly six unordered pairs, hence exactly six rules:

**Annihilations** (same symbol meets itself):

- **γγ** — both agents vanish; aux ports connect **index-swapped**
  (x₁–y₂, x₂–y₁). Drawn between the two mirrored triangles this is the
  **parallel arcs** picture (index and drawing invert because the redex
  partners face each other — a convention trap this project fell into once:
  2026-07-13, the author caught γγ behaving like δδ on screen).
- **δδ** — both vanish; aux ports connect **index-straight** (x₁–y₁, x₂–y₂) —
  the **crossing wires** picture.
- **εε** — both vanish; nothing remains.

The γ/δ asymmetry is not a detail — it is what makes the system expressive
enough to be universal. In the demo's Spec it is the `"swap": true` flag on
the γγ rule (a `maiz::reduce` extension; the upstream contract has one
annihilation flavor — reported in `MESSAGE_FOR_VOIDCORE.md`).

**Commutations** (different symbols pass through each other): the general
picture is *a meets b → arity(b) copies of a and arity(a) copies of b*, wired
in a grid, copies' principals facing outward along the wires the redex's aux
ports held.

- **γδ** — 2 δ-copies and 2 γ-copies in the 2×2 criss-cross grid (the
  textbook drawing this demo renders when `rot:"auto"` makes the copies face
  away from each other).
- **γε** — γ vanishes, ε copies onto each of γ's two aux wires (erasure
  propagating). Formally: 2 copies of ε, 0 copies of γ.
- **δε** — same shape: 2 ε copies, the δ erased.

# The properties (why this system is worth demonstrating)

- **Locality** — a rewrite touches only the two agents of the redex and their
  wires. Nothing at a distance can matter. (Conformance case 09 pins a
  *stronger, deliberately restricted* discipline: a redex wired to ITSELF is
  rejected. That self-wiring is a legal net — the author built one by hand on
  2026-07-13 and it crashed the app — so `maiz::reduce` now resolves internal
  redex wires correctly by default: annihilation chases the wire equations
  through the redex (a closed loop vanishes), commutation wires the
  corresponding copies' principals together. The contract's strict behavior
  survives behind a `strict_locality` flag, which only the conformance runner
  passes.)
- **Strong confluence (the diamond)** — distinct redexes are disjoint, so if a
  net reduces one step two different ways, both results converge in at most
  one more step. Consequence: **reduction order is irrelevant** — same normal
  form, same number of steps. This is why the demo's `reduce` may fire redexes
  in any order and the conformance suite checks convergence across randomized
  schedules (case 05).
- **No termination guarantee** — nets can grow forever (δδ-style feedback
  loops); the executor carries a termination guard (case 07), and that is a
  property of the *net*, not a bug in the engine.
- **Universality** — Lafont's 1997 result: *any* interaction net system can be
  translated into these three combinators. The demo is therefore not a toy
  instance of the framework; it is the framework's universal core.

# Vicious circles

A **vicious circle** is a cyclic configuration that can never reduce: the
classic form is a chain of agents where each principal port is wired to an
*auxiliary* port of the next, closing a cycle — every agent waits on the next,
no principal ever meets a principal, no redex ever forms. (The degenerate form
is a wire closed on itself.) Lafont's own term; a net containing one is
**deadlocked** in that region, and no rule — locality! — can ever free it.

They are **legal nets**. A correct editor must allow building them (the
author's ruling, 2026-07-13: *detectable, not prevented*). Detection is a walk:
follow principal→aux wires as a directed relation and look for cycles; a
"vicious circle" badge/tint is view work, planned in Void Maiz's backlog.

# The demo's mapping (maths object → model object)

| mathematics | this app |
|---|---|
| agent | rune with glyph γ/δ/ε (`signatures`: 2/2/0) |
| principal port | port 0 (apex/circle); the `0` in a relation |
| auxiliary port | ports 1..n (`a`, `b`) |
| wire | `link … --relation i:j` (undirected in the net reading; `to_net` ignores direction) |
| active pair | fettuccine (`0:0`) whose glyphs have a rule — the hot halo |
| passive wire | any other wire, incl. principal↔aux (draggable since 2026-07-13) |
| the six rules | the demo's `Spec` (3 annihilate + 3 commute) |
| a reduction step | `step`: `maiz::reduce::step` → net diff → ONE `batch` (rm + mint + relink) |
| normal form | `active pairs: 0` |
| confluence | any click order of `step` reaches the same normal form |
| the reduction trace | the dispatcher log; `undo` walks it backward |

One consequence worth stating: because every port holds exactly one wire end,
**principal ports are single-occupancy** — the editor enforces this at gesture
time (linking a principal clears whatever wire held it). Auxiliary fan-out is
a deliberate UI relaxation for dataflow hosts; a *net* being reduced should
not rely on it.

# Tag pigments — nets as the software's own substrate (2026-07-13)

The author's design: **tags ARE colors.** Tag a node `red` and it tints red;
add `yellow` and it turns orange; `blue`+`yellow` make green — RYB paint
mixing. The point is *how*: the mixing is computed by **interaction nets
running invisibly behind the UI**. Each color tag is an arity-0 agent; two
pigments wired principal-to-principal form a redex; a **fuse rule** (a
`maiz::reduce` extension beyond Lafont's calculus: `a~b → into`, the pair
replaced by ONE agent adopting both boundaries) rewrites the pair into the
mixed pigment. The node's tag list folds through the net; the survivor's glyph
is the tint. Primaries mix to secondaries; every other pair muddies to brown —
one rule per unordered pair, the same confluence guard as the six rules.

Try it: `tag era1 +red +yellow` in the command bar (or the inspector's chip
editor) → the eraser turns orange; `tag era1 -red` → yellow. Logged, undoable,
replayable — because the *tags* are the state; the pigment net is pure
background computation (the compute boundary: the app computes, the mantle
never sees the mixing net). When color tags are present they outrank
`hints.color`/`content.color`. Agents get **birth pigments** in the starter
net (γ +orange, δ +blue, ε +red) so their visible color is editable state
from the first frame.

**Pigment inheritance through rewrites** (2026-07-13, from the author finding
colors lost after an interaction): the CONTRACT says rewrite copies start
tagless, and the engine honors that. This *app* chooses to re-tag minted
copies from their same-glyph redex parent — a colored ε consumed by a γε
commute yields copies carrying its tags, as visible `tag` commands inside the
step batch. Demo policy layered above the contract, not a change to it.

# References

- Y. Lafont, **Interaction Combinators**, Information & Computation 137(1),
  1997 — the three agents, six rules, universality.
- Y. Lafont, **Interaction Nets**, POPL 1990 — the general framework.
- D. Mazza, **A Denotational Semantics for the Symmetric Interaction
  Combinators** (2007) and his survey work — the modern theory landscape.
- The executable contract: `../VoidCore/conformance/reduce/` (README + 10
  pinned cases); this repo's engine is `maiz::reduce`, conformance-proven.
