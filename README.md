# companalyze
Rewrite some test programs to analyze data compressibility

Port/Testing btrfs compression heuristic code

```
  ~$ make
  ~$ ./companalyze.out <file>

   ...
  8180. BSize: 131072, RepPattern: 0, BSet: 256, BCSet: 220, ShanEi%: 98| 99 ~1.0%, RndDist:     0, out: -3
  8181. BSize: 131072, RepPattern: 0, BSet: 256, BCSet: 221, ShanEi%: 99| 99 ~0.0%, RndDist:     0, out: -3
  8182. BSize: 131072, RepPattern: 0, BSet: 256, BCSet: 220, ShanEi%: 98| 99 ~1.0%, RndDist:     0, out: -3
  8183. BSize: 131072, RepPattern: 0, BSet: 256, BCSet: 220, ShanEi%: 99| 99 ~0.0%, RndDist:     0, out: -3
  8184. BSize: 131072, RepPattern: 0, BSet: 256, BCSet: 221, ShanEi%: 99| 99 ~0.0%, RndDist:     0, out: -3
  8185. BSize: 131072, RepPattern: 0, BSet: 256, BCSet: 219, ShanEi%: 98| 99 ~1.0%, RndDist:     0, out: -3
  ...
```
