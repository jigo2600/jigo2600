# TIA simulation

[TOC]

This section describes in some detail the problems in simulating the TIA efficiently and yet accurately.

As in any piece of electronics, electrical signals in the chip vary continuously over time. However, the chip is designed to make such continuous changes equivalent to a sequence of discrete logic operations. Hence, simulating the chip can be done at the level of the logic it implements, which is much simpler and faster than simulating analogue electrical signals and components.

Discretization means that signal levels are interpreted as boolean values (0 or 1), which in the TIA nominally corresponding to electrical levels 0V and 5V. In reality, electical signals change continuously over time and such changes take time to propagate in the circuit before components reach "stable" 0 and 5V values. Thus, in order to implement a *sequence* of operations,  the chip must have an internal mechansim to wait long enough for signals to stabilize to their new value before they can be used in a subsequent calculation. This mechanism is the *clock signal*, which divides time in *clock cycles*. Changes are initiated by the raising edge of a clock signal and stabilize to well defined logic values by the end of the cycle.

The main TIA clock is the *color clock* `CLK`. The TIA contains a number of other clock signals that are either in phase or in opposition of phase with `CLK` (in reality they are likely somewhat off-phase due to propagation delays). Some of these clocks are also used to drive dual-phase clock generators in certain sections of the chip.

Clocks syncrhonize updates to the state of the chip. In the TIA, the **state** is stored in:

* Edge-sensitive elements:
  * **Synchronous latches**, updated at the raising or falling edge of `CLK` .
* Level-sensitive elements:
  * **Asynchronous latches**, updated when their inputs change.
  * **Capacitors**,  usually part of dual-phase sections of the circuit which are updated *while* a specific phase lasts (generally phases last for the duration of a clock cycle).


### Updating the state

Simulating a clock cycle can then be broken down in:

1. Simulating the **raising edge** of `CLK` at the beginning of the cycle. This updates the edge-sensitive elements (synchronous flip-flops) that are sensitive to the raising clock edge.
2. Simulating the **high level** period of CLK during the first half of the of the cycle. This  updates the level-sensitive elements (the asynchronous latches and the capacitors) that are sensitive to the high level of the clock or to another relevant change to their input.
3. Simulating the **falling edge** of `CLK` in the middle of the cycle. This updates the synchronous flip-flops that are sensitive to the falling clock edge.
4. Simulating the **low level** period of CLK during the second half of the of the cycle. This  updates the asynchronous latches and the capacitors that are sensitive to the low level of the clock.

In general, the chip design assures that the results of these updates are predictable according to a deterministic logic; however, the TIA has the occasional race condition that can make prediction difficult in a few special cases.

Let $s_t$ be the state of the CPU at time $t$, where $t=0$ corresponds to the raising edge of the first clock cycle simulated, $t=1$ the raising edge of the second, and so forth. We can thus conceptually break down the simulation of the $t+1$-th cycle in:

1. Raising edge: computing the state $s_{t+\epsilon}$ right after the raising edge from the state $s_{t-\epsilon}$ right before the raising edge.
2. High level: computing the state $s_{t+1/2-\epsilon}$ from $s_{t+\epsilon}$.
3. Falling edge: computing the state $s_{t+1/2+\epsilon}$ form $s_{t+1/2-\epsilon}$.
4. Low level: computing the state $s_{t+1-\epsilon}$ from $s_{t+1/2+\epsilon}$.

The state is represented by a collection of boolean variables or bits containing the discretized values of electrical nodes, most of which correspond directly to the state elements discussing below (as any other node value can be inferred from those on the fly). Sometimes, as in the case of a counter, groups of bits are represented together in an integer.

Hence, if `s` is the state of all the simulated nodes, the first two steps look like this:

1. Set `snew` to be a copy of `s`. Then, for each node `X` driven by a raising-edge sensitive element, compute `snew[X]` as a function of `s`, and eventually replace `s` with `snew`.
2. For each node `X` driven by a level-sensitive element, recompute `s[X]` as a function of `s` to reflect any relevant change in the element input. Keep repeating 2 until no more changes to `s` are observed.

The last two steps are analogous. Note that there is a fundamental difference between step 1 and 2 in that in 1 the change to the state is *instantaneous* whereas in the second case it is *continuous*:

1. Implementing the first requires to clearly split the state between before and after, which breaks any possibility of oscillations for cyclic updates of the type $a\leftarrow b$ and $b\leftarrow a$ . In particular, if such a dependency exists, the effect of the update is to swap the values of $a$ and $b$. The order in which variables `X` are updated is irrelevant, but for certain special orders it is usually possible to implement updates *in-place* to save explicitly copying the state (see below) -- naturally this does not work for a cyclic update.
2. In the second case, such cyclic updates are avoided implicitly by the design of the logic, which guarantees that after a finite number of updates the state stabilizes (instead of oscillating indefinitely). This is obtained by guaranteeing that cyclic paths such as   $a\leftarrow b$ and $b\leftarrow a$ never form, for instance by using dual-phase clock signals. Instead, updates are chains of the type $a\leftarrow b \leftarrow c$ ; this also means that the change can be computed in one step by properly sorting the nodes `X` (in this case $b$ first, then $a$).

### In-place updates

A possible design for the simulator is to simulate the four steps above, producing every time a new state $s_t$ from a previous state $s_q$. This would work, but it is expensive as in practice most of the state does *not* change during such transitions. Instead, the simulator updates the state *in place*, so that only the bits that actually change need to be touched.

While this sounds trivial, in practice it needs careful coding. Consider for example the sequence $c = FF(b)$ and $b = FF(a)$ where $FF$ is a flip-flop sensitive to the raising edge of the clock. This circuit moves the value of $(a,b)$ to $(b,c)$ at the edge. Compare the three implementations:

```
// In-place (correct)
1. c = b
2. b = a

// In-place (incorrect)
1. b = a
2. c = b

// Not in-place (correct, the update order does not matter)
1. b[t+1] = a[t]
2. c[t+1] = b[t]
```

### Modules

The TIA simulator is broken down in several classes that simulate specific circuit modules. Some simulate **edge-sensitive** logic and some **level-sensitive** logic. The distinction is rather important as, based on the discussion above, edge-sensitive interconnected modules should usually be updated in reverse dependency order (see in-place updates), whereas the level-sensitive modules  in the forward dependency order. Another distinction is that a level-sensitive update function is idempotent: it can be called several times in a row with the same argument without changing the result; an edge-sensitive update function instead may transition to a new state every time.

#### Dual-phase clock `TIADualPhase`

 `TIADualPhase` simulates a [dual-phase clock generator](TIA_Modules#dual-phase). It is driven by a clock signal `CLK` and a reset signal `RES` and outputs the dual-phase levels $(\Phi_1,\Phi_2)$ and the reset latch `RESL`.  

The method `cycle`  simulates a clock cycle  $[t,t+1)$, starting with the state $s_{t-\epsilon}$ right before the rising edge of `CLK` and ending with the state $s_{t-\epsilon+1}$ right before the next raising edge. It takes two arguments:

* A boolean `CLK` telling whether during this clock cycle the clock pulse actually occurs or is instead suppressed.
* A boolean `RES` telling whether the signal `RES` is high or low.

If `CLK` is high `cycle()` advances the phase by one step. This behavior is edge-sensitive: the change is not instantaneous at the beginning of the cycle because it is driven by edge-sensitive flip-flops. However, if the clock pulse is suppressed, then the phase does not change and the behavior of `cycle()` is level-sensitive and idempotent.

Another exception is the reset condition. In the circuit, `RES` is a level-sensitive input, in the sense that it takes effect as soon as it is raised, overriding the clock and immediately setting the phase to zero and latching `RESL` high. The duration of the `RES` pulse does not matter as the effect sticks, so the only relevant parameter is whether `RES` is or goes high at all during the cycle and, if so, when this occurs.

Since changes to `RES` are potentially asynchronous with `CLK` , `RES` may be raised some time $t' > t$ after the clock edge. In this case, the nodes $(\Phi_1,\Phi_2)$ may undergo *two* transitions during the clock cycle, one at $t$ and one at $t'$, which may be an unintended effect or race condition.

In the simulator, thus, it is generally **assumed** that `RES` has only one transition low to high in $[t,t+1)$ and that this occurs at time $t'=t$.

#### Dual-phase delay `TIADelay`

 `TIADelay` simulates a [dual-phase delay D1](TIA_Modules#d1), [D2](TIA_modules#d2) and  [D1R](TIA_modules#d2). The module is updated by `cycle()` which is level-sensitive, idempotent, and takes as input `TIADualPhase` , the data node `D` the reset node `R` . 

All the inputs are level-sensitive. However, the value of `D` is sampled only during a cycle $\Phi_1=1$ and is reflected in the output `Q` only two cycles later, when $\Phi_2=1$.  Since`D` is in turn driven by some other dual-phase logic module, the signal changes when $\Phi_2$ goes high, but is stable and constant by the time $

Phi_1$ and is held constant while 

The output is the delayed data node `Q`. `cycle()` simulates clock cycle $[t,t+1) $ so that:

* `TIADelay` ,`D` and `R` are the levels of the input nodes in  $[t,t+1) $ and in response
* `Q` is the level of the output signal in $[t,t+1)$.

#### Dual-phase extra clock `TIAExtraClock`

`TIAExtraClock` simulates the extra clock generation circuit for one visual object. The `clock()` function simulates a clock period, it is level-sensitive and idempotent and takes as input `TIADualPhase` , the `SEC` node and the `HMC` strobe. The class also contains the `HM` register, which is updated via `setHM`. The output is the `ENA` flag that enables the extra clock pulse.

