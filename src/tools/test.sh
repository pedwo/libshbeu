#!/bin/ash
#
# Script to test BEU operations using the tools
#
# NB: With shbeu-display the destination surface has the framebuffer format
# and will always be registered with UIOMux as contiguous memory.

# One layer
for l1 in 888 rgb yuv x888; do \
  shbeu-display -s vga -i vga.${l1}; \
done

# Two layers
for l1 in 888 rgb yuv x888; do \
  for l2 in 888 rgb yuv x888; do \
    shbeu-display -s vga -i vga.${l1} -s qvga -i qvga.${l2}; \
  done; \
done

# Three layers
for l1 in 888 rgb yuv x888; do \
  for l2 in 888 rgb yuv x888; do \
    for l3 in 888 rgb yuv x888; do \
      shbeu-display -s vga -i vga.${l1} -s qvga -i qvga.${l2} -s qcif -i qcif.${l3}; \
    done; \
  done; \
done

