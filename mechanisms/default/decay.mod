NEURON {
    SUFFIX decay
    USEION x WRITE xd
}

INITIAL { F = xd }

STATE { F }

BREAKPOINT {
   SOLVE dF METHOD cnexp
   xd = F
}

DERIVATIVE dF {
   F = xd
   F' = -0.05*F
}
