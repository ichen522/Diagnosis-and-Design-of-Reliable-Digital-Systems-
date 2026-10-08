# Do NOT change this file.

import os
import subprocess as sub
import pdb
from collections import Counter

WEB_ADDR = "https://sportlab.usc.edu/~msabrishami/files/"
WEB_DIR = "ee658"
TEST_DIR = ".auto-tests-parser"
ZIP_NAME = "auto-tests-parser.zip"

class bcolors:
    OKGREEN = '\033[92m'
    FAIL = '\033[91m'
    WARNING = '\033[93m'
    ENDC = '\033[0m'

def prepare_env():
    """download tests/create folders"""

    os.system(f"rm -rf {TEST_DIR}")
    os.system(f"mkdir {TEST_DIR}")
    #os.system(f"rm -rf {ZIP_NAME}")

    #if os.path.exists("/etc/hostname") and \
    #        ("viterbi-scf" in open("/etc/hostname").readline().strip()):
    #    os.system(f"cp /home/{WEB_DIR}/{ZIP_NAME} ./")
    #else:
    #    os.system(f"wget {WEB_ADDR}/{WEB_DIR}/{ZIP_NAME}")
    
    os.system(f"unzip {ZIP_NAME} -d ./{TEST_DIR} > /dev/null") 
    #os.system(f"rm -rf {ZIP_NAME}")


def generate_tests():
    
    os.system(f"mkdir {TEST_DIR}/tests")
    ckts = []
    for ckt in os.listdir(f'./{TEST_DIR}/input-ckts/'):
        ckt =  ckt.replace('.ckt','')
        with open(f"{TEST_DIR}/tests/{ckt}.txt", 'w') as ftest:
            ftest.write(f'read ../{TEST_DIR}/input-ckts/{ckt}.ckt\n')
            ftest.write(f'lev ../{TEST_DIR}/outputs/{ckt}_lev.txt\n')
            ftest.write('quit')
        print(f"Test file for {ckt} generated")
        ckts.append(ckt)

    return ckts


def correct_answer(answer, golden):
    with open(golden, 'r') as golden_file:
        golden_result = Counter(line for line in golden_file if line.strip())
    with open(answer, 'r') as answer_file:
        answer_result = Counter(line for line in answer_file if line.strip())

    for line, count in (answer_result - golden_result).items():
        print(f"{bcolors.FAIL}\nWrong line in output:")
        print(f"{bcolors.FAIL}\t{line.rstrip()} (x{count})")

    for line, count in (golden_result - answer_result).items():
        print(f"{bcolors.FAIL}\nMissing line in output:")
        print(f"{bcolors.FAIL}\t{line.rstrip()} (x{count})")

    return answer_result == golden_result


if __name__ == '__main__':

    prepare_env()
    ckts = generate_tests()

    pass_all = True
    failed_count = 0
    os.system("rm -rf build; mkdir build && cd build; cmake ..; make")
    
    if not os.path.exists('./build/levelizer'):
        print('cpp file not compiled.')
        exit(1)


    for ckt in ckts:
        try:
            gold_fail = False
            print('_'*50)
            print('Testing lev() on ' + ckt)
            os.system(f"cd build/; ./levelizer < ../{TEST_DIR}/tests/{ckt}.txt > /dev/null")
            golden_result = f"{TEST_DIR}/golden-outputs/{ckt}.log"

            if not os.path.exists(golden_result):
                print(f'\n{bcolors.WARNING}Golden result does not exists.',f"{bcolors.ENDC}")
                gold_fail = True

            if correct_answer(f"{TEST_DIR}/outputs/{ckt}_lev.txt", golden_result):
                print(f"{bcolors.OKGREEN}\nTest '{ckt.replace('.ckt','')}'","PASSED",f"{bcolors.ENDC}")

            else:
                print(f"{bcolors.FAIL}\nTest '{ckt.replace('.ckt','')}'","FAILED",f"{bcolors.ENDC}")
                pass_all = False
                failed_count+=1

        except Exception as e:
            print(f"{bcolors.FAIL}\nError in readckt.cpp{bcolors.ENDC}")
            if not gold_fail:
                pass_all = False
                failed_count+=1
            
    print('-'*50)
    os.system(f"rm -rf {TEST_DIR}")

    if pass_all:
        print(f'{bcolors.OKGREEN}All tests passed.')
        exit(0)
    else:
        print(f'{bcolors.FAIL}{failed_count} tests are failed.')
        exit(1)
