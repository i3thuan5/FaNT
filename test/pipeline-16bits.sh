set -e

cat example/57353.raw | ./filter_add_noise -n example/subway.raw -u -s 10 -r 2000 -e fant.log > output.raw
cmp output.raw test/8bits.raw
