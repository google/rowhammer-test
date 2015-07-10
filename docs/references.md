
# References on the rowhammer problem

* Paper: "[Flipping Bits in Memory Without Accessing Them: An
  Experimental Study of DRAM Disturbance
  Errors](http://users.ece.cmu.edu/~yoonguk/papers/kim-isca14.pdf)",
  Yoongu Kim, Ross Daly, Jeremie Kim, Chris Fallin, Ji Hye Lee,
  Donghyuk Lee, Chris Wilkerson, Konrad Lai, Onur Mutlu, 2014.

* Paper: "[Active-Precharge Hammering on a Row Induced Failure in DDR3
  SDRAMs under 3x nm
  Technology](http://rsc.hanyang.ac.kr/homepage_v2/journal/KyungbaePark_et_al_Active-Precharge%20Hammering%20on%20a%20Row%20Induced%20Failure%20in%20DDR3%20SDRAMs%20under%203x%20nm%20Technology.pdf)",
  Kyungbae Park, Sanghyeon Baeg, ShiJie Wen, Richard Wong, 2014.

* Paper: "[Architectural Support for Mitigating Row Hammering in DRAM
  Memories](http://users.ece.gatech.edu/~pnair6/rowhammer/rowhammer.pdf)",
  Dae-Hyun Kim, Prashant J. Nair, and Moinuddin K. Qureshi, 2014.

  * Short, 4-page paper.

  * Discusses fixes: Counter-Based Row Activation (CRA) versus
    Probabilistic Row Activation (PRA).

  * "Below the feature size of 100nm, DRAM cell transistors suffer
    from short channel effect (SCE), which lowers threshold voltage,
    increases leakage, and reduces the retention time of DRAM
    cells. To overcome SCE and maintain the retention time, DRAM
    vendors now exploit three-dimensional (3D) cell
    transistors. However, such 3D cell transistors severely suffer
    from the activation of adjacent rows potentially causing data
    errors to neighboring rows."

  * "Row hammering originates from two effects:
    * Word line to Word line (WL-WL) Coupling
    * Passing-Gate Effect"

  * Figure 2 is a graph showing the degree to which various benchmarks
    cause accidental row hammering via cached memory accesses.
