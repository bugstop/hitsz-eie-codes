import threading
import time
from datetime import datetime
import numpy as np
import logging
import struct

training_image_file = './source/train-images.idx3-ubyte'
training_label_file = './source/train-labels.idx1-ubyte'
test_image_file = './source/t10k-images.idx3-ubyte'
test_label_file = './source/t10k-labels.idx1-ubyte'


class Log(object):
    def __init__(self):
        logging.basicConfig(level=logging.INFO, filemode='w', filename='knn-1.2min.txt', format='%(message)s')
        console = logging.StreamHandler()
        console.setLevel(logging.INFO)
        formatter = logging.Formatter('%(message)s')
        console.setFormatter(formatter)
        logging.getLogger('').addHandler(console)

    @staticmethod
    def info(msg):
        logging.info(f'{datetime.now().strftime("%b%d/%y, %H:%M:%S.%f")}: {msg}')


def samples(file_img, file_lb, num):
    with open(file_img, 'rb') as f:
        images = f.read()[16:]
    with open(file_lb, 'rb') as f:
        labels = f.read()[8:]

    sample = []
    offset = 0
    for i in range(num):
        x = np.array(struct.unpack_from('>784B', images, offset)).reshape((28, 28))
        offset += struct.calcsize('>784B')
        y = labels[i]
        sample.append([x, y])
    return sample


def difference(x1, x2):
    diff_0 = np.sum((x1[4:24, 4:24] - x2[4:24, 4:24]) ** 2)
    diff_r1 = np.sum((x1[4:24, 4:24] - x2[4:24, 5:25]) ** 2)
    diff_r2 = np.sum((x1[4:24, 4:24] - x2[4:24, 6:26]) ** 2)
    diff_l1 = np.sum((x1[4:24, 4:24] - x2[4:24, 3:23]) ** 2)
    diff_l2 = np.sum((x1[4:24, 4:24] - x2[4:24, 2:22]) ** 2)
    s = min(diff_0, diff_l1, diff_l2, diff_r1, diff_r2)
    return s


log = Log()

log.info('Read Files')

number_of_tests = 10000
number_of_threads = 8
samples_per_thread = number_of_tests // number_of_threads
train_data = samples(training_image_file, training_label_file, 60000)
test = samples(test_image_file, test_label_file, 10000)

lock = threading.Lock()
count_accurate = 0
count_total = 0

log.info('Testing')


def subprocess(start, number, lk):
    global count_accurate, count_total
    for m in range(start + 1, start + number + 1):
        test_x, test_y = test[m - 1]
        min_x, min_y = float('inf'), 0

        for n in range(60000):
            train_x, train_y = train_data[n]
            curr_x = difference(np.array(test_x),
                                np.array(train_x))
            if curr_x < min_x:
                min_x = curr_x
                min_y = train_y

        result = min_y == test_y

        lk.acquire()
        if result:
            count_accurate += 1
        count_total += 1
        lk.release()

        log.info(f'     Sample: {m:04}: {test_y}->{min_y} = {result:1}   - {count_accurate}/{count_total}')


for t in range(number_of_threads):
    th = threading.Thread(target=subprocess, args=(samples_per_thread * t, samples_per_thread, lock,))
    th.start()

th.join()
time.sleep(8)
log.info(f'Accuracy: {count_accurate / count_total * 100}%')
