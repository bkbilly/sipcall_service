from typing import Union
import time
import sys
from threading import Thread
import subprocess

import yaml
from fastapi import FastAPI


running = False
process = None
conf = yaml.load("/opt/sipcall/config.yaml", Loader=yaml.FullLoader)


def run_command(command):
    if running:
        # process.kill()
        print("Terminating command...")
        process.terminate()
        process.wait()
        # time.sleep(2)
    running = True
    process = subprocess.Popen(command)
    running = False

app = FastAPI()


@app.get("/")
def read_root():
    return {"Hello": "World"}


@app.get("/makecall/{phone}")
def read_item(phone: str, audiofile: str = "/opt/sipcall/audio/play2.wav"):
    command = ["/bin/sipcall",
        "-sd", conf["domain"],
        "-su", conf["username"],
        "-sp", conf["password"],
        "-so", conf["port"],
        "-p", phone,
        "-s", "1",
        "-r", conf["repeat"],
        "-a", audiofile,
        "-t", conf["timeout"],
    ]
    Thread(target=run_command, args=(command,), daemon=True).start()
    
    # result = subprocess.run(
    #     command,
    #     stdout=subprocess.PIPE,
    #     stderr=subprocess.PIPE,
    #     check=False,
    #     timeout=100,
    # )
    # print(result.stdout)
    # print(result.stderr)
    return {"phone": phone, "audiofile": audiofile, "command": command}
