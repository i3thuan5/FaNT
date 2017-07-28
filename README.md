# FaNT - Filtering and Noise Adding Tool

[![Build Status](https://travis-ci.org/i3thuan5/FaNT.svg?branch=master)](https://travis-ci.org/i3thuan5/FaNT)

This is a fork version of [FaNT](http://dnt.kr.hs-niederrhein.de/indexbd2f.html?option=com_content&view=article&layout=default&id=22&Itemid=15&lang=en) from Niederrhein (H.-G. Hirsch’s Lab). This version supports data pipeline.

## Install
```
git clone https://github.com/i3thuan5/FaNT.git
cd FaNT
make -f filter_add_noise.make
./filter_add_noise -h # The executable file
```

## Usage
### Pipeline
```
cat example/57353.raw | ./filter_add_noise -n example/subway.raw -u -s 10 -r 2000 -e fant.log > output.raw
```

### Batch Mode
List source files in `example/in.list`, target files in `example/out.list`
```
./filter_add_noise -i example/in.list -o example/out.list -n example/subway.raw -u -s 10 -r 2000 -e fant.log
```

For more detail, see [the documentation](https://github.com/i3thuan5/FaNT/blob/master/fant_manual.pdf).

## The Calculation of SNR
See [the comment](https://raw.githubusercontent.com/i3thuan5/FaNT/master/snr_comments.html).
