import argparse
import codecs
import json
import os
import subprocess
import sys
from concurrent import futures
import shutil
import glob

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        required=False,
        default=False,
        help="Enable verbose output",
    )
    parser.add_argument(
        "--cleanup",
        action="store_true",
        required=False,
        default=False,
        help="Clean temporary files",
    )
    parser.add_argument(
        "--speech",
        type=str,
        required=True,
        default=None,
        help="Path containing clean speech files",
    )
    parser.add_argument(
        "--noise",
        type=str,
        required=True,
        default=None,
        help="Path containing noise files",
    )
    parser.add_argument(
        "--output",
        type=str,
        required=True,
        default=None,
        help="Path to save output files",
    )
    parser.add_argument(
        "--temp_dir",
        type=str,
        required=True,
        default=None,
        help="Path to save intermediate files",
    )
    parser.add_argument(
        "--snr",
        type=str,
        required=True,
        default=None,
        help="Comma separated list of SNRs",
    )

    args = parser.parse_args()

    wav_files = []
    if args.speech:
        speech_wav_files = sorted(glob.glob(args.speech + "/**/*.wav", recursive=True))

    if len(speech_wav_files) == 0:
        print(f"No speech files found at {args.speech}")
        sys.exit()

    if not os.path.exists(args.temp_dir):
        os.makedirs(args.temp_dir, exist_ok=True)

    speech_file_list = os.path.join(args.temp_dir, "speech_files.txt")
    mixed_file_list = os.path.join(args.temp_dir, "mixed_files.txt")
    noise_file_raw = os.path.join(args.temp_dir, "noise.raw")
    temp_speech_path = os.path.join(args.temp_dir, "speech")
    if args.noise:
        if os.path.isfile(args.noise):
            noise_file = args.noise
        else:
            files = sorted(glob.glob(args.noise + "/**/*.wav", recursive=True))
            if len(files) == 0:
                print(f"No noise files found at {args.noise}")
            elif len(files) > 1:
                command = (
                    "sox "
                    + " ".join(files)
                    + " "
                    + os.path.join(args.temp_dir, "noise_combined.wav")
                    + " rate 16000 channels 1"
                )
                print(command)
                if subprocess.call(command, shell=True) != 0:
                    print(f"Concating noise files failed.")
                    sys.exit()
                else:
                    noise_file = os.path.join(args.temp_dir, "noise_combined.wav")
            else:
                noise_file = files[0]

            command = "sox " + noise_file + " " + noise_file_raw
            print(command)
            if subprocess.call(command, shell=True) != 0:
                print(f"Generating raw noise file failed.")
                sys.exit()

    if not os.path.exists(noise_file_raw):
        print(f"No noise file to mix")
        sys.exit()

    def copy_convert_raw(src, dst):
        # Convert speech to raw
        # print(f"{src}->{dst}")
        if src.endswith(".wav"):
            dst_raw = dst.replace(".wav", ".raw")
            command = "sox " + src + " " + dst_raw
            print(command)
            if subprocess.call(command, shell=True) != 0:
                print(f"Generating raw speech files failed.")
                sys.exit()
            print("copy_convert_raw exit")

    # defining the function to ignore the files
    # if present in any folder
    def ignore_files(dir, files):
        # print(dir, files)
        return [f for f in files if os.path.isfile(os.path.join(dir, f))]

    # create dir structure in output
    shutil.copytree(args.speech, args.output, ignore=ignore_files)
    # dirs_exist_ok=True,

    # Generate list of raw speech files
    command = "find " + args.speech + ' -name "*.wav" > ' + speech_file_list
    print(command)
    if subprocess.call(command, shell=True) != 0:
        print(f"Generating raw speech files list failed.")
        sys.exit()

    # Generate list of raw output files
    command = (
        "sed 's#"
        + args.speech
        + "#"
        + args.output
        + "/#g' "
        + speech_file_list
        + " > "
        + mixed_file_list
    )
    print(command)
    if subprocess.call(command, shell=True) != 0:
        print(f"Generating raw noise mixed speech files list failed.")
        sys.exit()

    command = (
        "./filter_add_noise"
        + " -i "
        + speech_file_list
        + " -o "
        + mixed_file_list
        + " -n "
        + noise_file_raw
        + " -u -m snr_8khz -r 12345"
        + " -s "
        + args.snr
    )
    print(f"Executing FaNt command: {command}")
    if subprocess.call(command, shell=True) != 0:
        print(f"FaNt failed.")
        sys.exit()

    if args.cleanup:
        print("Removing temporary files")
        shutil.rmtree(args.temp_dir)
