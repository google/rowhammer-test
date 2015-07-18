
# References on the rowhammer problem

* Paper: "[Flipping Bits in Memory Without Accessing Them: An
  Experimental Study of DRAM Disturbance
  Errors](http://users.ece.cmu.edu/~yoonguk/papers/kim-isca14.pdf)",
  Yoongu Kim, Ross Daly, Jeremie Kim, Chris Fallin, Ji Hye Lee,
  Donghyuk Lee, Chris Wilkerson, Konrad Lai, Onur Mutlu, 2014.

  * Presented at ISCA (ACM/IEEE International Symposium on Computer
    Architecture), 14th-18th June 2014.

  * Cites "Alternate Hammering Test for Application-Specific DRAMs".

* Paper: "[Active-Precharge Hammering on a Row Induced Failure in DDR3
  SDRAMs under 3x nm
  Technology](http://rsc.hanyang.ac.kr/homepage_v2/journal/KyungbaePark_et_al_Active-Precharge%20Hammering%20on%20a%20Row%20Induced%20Failure%20in%20DDR3%20SDRAMs%20under%203x%20nm%20Technology.pdf)",
  Kyungbae Park, Sanghyeon Baeg, ShiJie Wen, Richard Wong, 2014.

  * Short, 4-page paper.

  * By "3x nm" they mean between 30 and 40 nm.

  * "Here, we mainly discuss the experimental results of the
    commercial DDR3 components from three major memory
    vendors. ... For a vendor memory component, a cell started to fail
    after only 98K accesses to a row, which is about 7.54% of the
    specification-permitted accesses of 1,300K."

  * Mentions a cause being "the parasitic coupling effect".

  * Uses the term "active-precharge hammering on a row" (APHR) for
    "row hammering".  "Active" and "precharge" refer to the commands a
    memory controller sends to a DRAM module for activating and
    closing rows.

  * Presented at the IEEE International Integrated Reliability
    Workshop, 16th October 2014.  It is shown as "late news" in the
    [workshop's
    schedule](http://www.iirw.org/fileadmin/user_upload/2014progScheduleV333-1014.pdf).

  * Cites "Alternate Hammering Test for Application-Specific DRAMs".

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

  * Published in Computer Architecture Letters.

  * Cites Yoongu Kim et al's paper (cited as a preprint).

## Non-open-access papers

* Paper: "[Alternate Hammering Test for Application-Specific DRAMs and
  an Industrial Case
  Study](http://ieeexplore.ieee.org/stamp/stamp.jsp?tp=&arnumber=6241628)",
  2012, Rei-Fu Huang, Hao-Yu Yang, M. C. Chao, Shih-Chin Lin.

  * Abstract: "This paper presents a novel memory test algorithm,
    named alternate hammering test, to detect the pairwise word-line
    hammering faults for application-specific DRAMs. Unlike previous
    hammering tests, which require excessively long test time, the
    alternate hammering test is designed scalable to industrial DRAM
    arrays by considering the array layout for potential fault sites
    and the highest DRAM-access frequency in real system
    applications. The effectiveness and efficiency of the proposed
    alternate hammering test are validated through the test
    application to an eDRAM macro embedded in a storage-application
    SoC."

  * Presented at the ACM/IEEE Design Automation Conference (DAC),
    3rd-7th June 2012.
