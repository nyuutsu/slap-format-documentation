# IPS — todos

Known gaps between the design in `questions.md` and the code in `src/Slap/IPS/`. Each item is work slap has committed to doing; priority and shape are noted where they're not obvious. Speculative stuff lives in `notebook.md`.

### change truncation-marker behavior: truncation is for shrinking only

right now, truncation marker sets the size of the file. if this implies undefined filecontents, the trailing mystery region is zero-filled. we disfavor this. two behaviors. one needs to be done. the other needs to be done if it isn't already in place.

1. truncation is for shrink. if marker is present and it won't result in shrinking (a fact only possible to know when slap knows the input file), marker is ignored and a warning is emitted about the funky patch construction & the decision made to ignore the marker

2. shrink wins. if the patch writes data across the whole file, or even does so and also appends data at the end, but also uses a truncation marker to shrink the file: the work is performed, then the part after the marker is thrown out. if work is thrown out (not 'shrinking happened', but 'work was done by the patch, but the patch then threw that same work out to respect the marker'), warn