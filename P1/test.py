# Do NOT change this file.

import os

WEB_ADDR = "https://sportlab.usc.edu/~msabrishami/files"
WEB_DIR = "ee658"
TEST_DIR = "auto-tests-phase1"
ZIP_NAME = f"{TEST_DIR}.zip"

TPI_DIR = f"{TEST_DIR}/tpi"
CIRCUIT_DIR = f"{TEST_DIR}/ckts"
OUTPUT_DIR = f"{TEST_DIR}/outputs"
GOLDEN_DIR = f"{TEST_DIR}/golden_results"
CMD_TESTS_DIR  = f"{TEST_DIR}/cmd_tests" 

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

    # if os.path.exists("/etc/hostname") and \
    #         ("viterbi-scf" in open("/etc/hostname").readline().strip()):
    #     os.system(f"cp /home/{WEB_DIR}/{ZIP_NAME} ./")
    # else:
    #     os.system(f"wget {WEB_ADDR}/{WEB_DIR}/{ZIP_NAME}")
    
    os.system(f"unzip {ZIP_NAME} -d ./{TEST_DIR} > /dev/null") 
    os.system(f"mkdir {CMD_TESTS_DIR}")
    # os.system(f"rm -rf {ZIP_NAME}")


def generate_tests():
    
    cmd_tests_files = []
    for ckt in os.listdir(CIRCUIT_DIR):
        for golden_result in os.listdir(GOLDEN_DIR):
            ckt =  ckt.replace('.ckt','')
            if golden_result.startswith(f'GOLD_{ckt}_'):
                t = int(golden_result.replace(f'GOLD_{ckt}_t', '').replace('_output.txt', ''))
                fout = f'{CMD_TESTS_DIR}/{ckt}_t{t}.txt'
                with open(fout, 'w') as ftest:
                    ftest.write(f'READ ../{CIRCUIT_DIR}/{ckt}.ckt\n')
                    ftest.write(f'LEV ../{TEST_DIR}/temp.txt\n')
                    ftest.write(f'LOGICSIM ../{TPI_DIR}/{ckt}_t{t}.txt ../{OUTPUT_DIR}/{ckt}_t{t}_output.txt\n')
                    ftest.write('QUIT')
                cmd_tests_files.append(f'{ckt}_t{t}.txt')
        
        os.system(f"rm -rf {OUTPUT_DIR}")
        os.system(f"mkdir {OUTPUT_DIR}")
    # os.system(f"rm -rf {ZIP_NAME}")
    return cmd_tests_files


def correct_answer(answer, golden):
    passed=True
    golden_result = open(golden,'r').readlines()
    answer_result = open(answer,'r').readlines()
    for line in answer_result:
        if line and line not in golden_result:
            print(f"{bcolors.FAIL}\n")
            print(f"{bcolors.FAIL}\nWrong line in output:")
            print(f"Error on\n\t{bcolors.FAIL}"+line)
            passed = False

    for line in golden_result:
        if line and line not in answer_result:
            print(f"{bcolors.FAIL}\nMissing line in output:")
            print(f"{bcolors.FAIL}\t"+line)
            passed = False

    return passed


if __name__ == '__main__':
    prepare_env()
    cmd_test_files = generate_tests()
    pass_all = True
    failed_count = 0
    passed_count = 0

    os.system("rm -rf build; mkdir build && cd build; cmake ..; make")
    # TODO check existence of simulator exe file right here

    for cmd_test in cmd_test_files:
        try:
            gold_fail = False
            print('_'*50)
            print('Testing logicsim on ' + cmd_test.replace('.txt', ''))
            os.system(f"cd build/; ./simulator < ../{CMD_TESTS_DIR}/{cmd_test} > /dev/null ")
            golden_result = f"{GOLDEN_DIR}/GOLD_{cmd_test.replace('.txt', '')}_output.txt"
            if not os.path.exists(golden_result):
                print(f'\n{bcolors.WARNING}Golden result does not exists.',f"{bcolors.ENDC}")
                gold_fail = True
            student_answer = f"{OUTPUT_DIR}/{cmd_test.replace('.txt', '')}_output.txt"
            if correct_answer(student_answer, golden_result):
                print(f"{bcolors.OKGREEN}\nTest '{cmd_test.replace('.txt','')}'","PASSED",f"{bcolors.ENDC}")
                passed_count += 1
            else:
                print(f"{bcolors.FAIL}\nTest '{cmd_test.replace('.txt','')}'","FAILED",f"{bcolors.ENDC}")
                pass_all = False
                failed_count+=1

        except Exception as e:
            print(e)
            print(f"{bcolors.FAIL}\nError in {bcolors.ENDC}")
            if not gold_fail:
                pass_all = False
                failed_count+=1
            
    print('-'*50)
    os.system(f"rm -rf {TEST_DIR}")

    print(f'{bcolors.OKGREEN}{passed_count} tests are passed.')
    print(f'{bcolors.FAIL}{failed_count} tests are failed.')

    if pass_all:
        exit(0)
    else:
        exit(1)
