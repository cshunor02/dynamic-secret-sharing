# Dynamic Proactive Secret Sharing

### Supervisor: Máté Gyarmati
### Programmer: Hunor Csapó

Secret sharing schemes are key elements of cryptography because they ensure a safe and secure way to distribute a given secret among a set of users. In this research, we constructed and analyzed a new dealerless DPSS scheme. We have measured its computational security, compared it to existing solutions, and made the correct assumptions.

Our solution implements the BFV homomorphic encryption using the [Microsoft SEAL](https://www.microsoft.com/en-us/research/project/microsoft-seal/) library. For the implementation, we have chosen C++ as our programming language, using [ZeroMQ](https://zeromq.org/) for synchronous and asynchronous node communications.

To be able to run the program, download the following packages to the same folder where the project was downloaded:
* [Microsoft SEAL](https://www.microsoft.com/en-us/research/project/microsoft-seal/) as `/seal` folder
* From [ZeroMQ](https://zeromq.org/), download `zmq.hpp`, `zmq_addon.hpp`, and `/libzmq`

## Start the program

After extracting the `zip` file that was downloaded from GitHub, follow the  instructions below:

1. Open a Linux terminal (on Windows, use [WSL](https://learn.microsoft.com/en-us/windows/wsl/install)).
2. Go to the `build` folder of the project (it will be empty).
3. Run `cmake -DCMAKE_BUILD_TYPE=Release ..` to create the CMake file that will be used for the compilation.
4. Run `make -j` to compile the project.
5. To start the program, run `./dynamic <N> <t> <modulo>` with the following parameters:

* `N` is mandatory, the number of participants.
* `t` is mandatory, the threshold, ($2 \le t \le N$).
* `modulo` is mandatory, the limit representing $\mathbb{F}$ field.

## Start the measures

After extracting the `zip` file, follow the instructions below:

1. Open a Linux terminal (on Windows, use [WSL](https://learn.microsoft.com/en-us/windows/wsl/install)).
2. Go to the `build` folder of the project (it will be empty).
3. Run `cmake -DCMAKE_BUILD_TYPE=Release ..` to create the CMake file, that will be used for the compilation.
4. Run `make -j` to compile the project.
5. To start the program, run `./automated_run.sh <N> <t> <modulo> [repetitions]` with the following parameters:

* `N` is mandatory, the number of participants
* `t` is mandatory, the threshold, ($2 \le t \le N$)
* `modulo` is mandatory, the limit representing $\mathbb{F}$ field.
* `repetitions` is the number that indicates how many times the program should perform the tasks (by default, it is `1`)

## Classes

* **Bulletin** - Manages the public bulletin board, collections of variables of user data.
* **Homomorphic** - Homomorphic encryptor class (for Microsoft SEAL), including share addition, encryption, and decryption, noise budget management.
* **Participant** - The core user class, it sends and receives ZeroMQ based messages, and executes the protocol's steps.
* **Polynomial** - Calculating the Lagrange basis polynomial.
* **Main** - Initialization of the threads and objects, and running the program