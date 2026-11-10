"""remaster-art/progress.json: every generation job (one picture or a sheet), its state and tries."""
import json, os

MAX_TRIES = 3


class Progress:
    def __init__(self, path):
        self.path = path
        self.jobs = json.load(open(path)) if os.path.exists(path) else {}

    def save(self):
        tmp = self.path + '.tmp'
        with open(tmp, 'w') as f:
            json.dump(self.jobs, f, indent=1)
        os.replace(tmp, self.path)

    def add(self, name, sha1s):
        if name not in self.jobs:
            self.jobs[name] = {'sha1s': list(sha1s), 'status': 'pending', 'tries': 0, 'reasons': []}
            self.save()

    def next(self):
        for name, j in self.jobs.items():
            if j['status'] in ('pending', 'failed'):
                return name
        return None

    def result(self, name, ok, reasons=()):
        j = self.jobs[name]
        j['tries'] += 1
        j['reasons'] = list(reasons)
        j['status'] = 'done' if ok else ('attention' if j['tries'] >= MAX_TRIES else 'failed')
        self.save()
