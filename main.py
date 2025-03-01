from threading import Thread
from typing import Union
import subprocess
import time
import sys

import yaml
from fastapi import FastAPI


with open("/opt/sipcall/config.yaml", "r", encoding="utf8") as file:
    conf = yaml.load(file, Loader=yaml.FullLoader)
print(conf["domain"])


def run_command(command):
    subprocess.Popen(command)

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
    return {"phone": phone, "audiofile": audiofile, "command": command}
