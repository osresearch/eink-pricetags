Images are 128x250 single bit.  They are mirrored due to the draw order
on the e-ink screen.

```
convert hello.png -flip  -depth 1 gray:hello.raw
```
