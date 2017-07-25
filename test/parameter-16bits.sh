set -e

./filter_add_noise -i example/in.list -o example/out.list -n example/subway.raw -u -s 10 -r 2000 -e fant.log
cmp example/57353_g712_sub_10db.raw test/16bits.raw
