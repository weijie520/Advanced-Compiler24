import sys
import json
from deepdiff import DeepDiff


def load_json(filepath):
    with open(filepath, "r") as f:
        return json.load(f)


json1 = load_json(sys.argv[1])
json2 = load_json(sys.argv[2])
diff = DeepDiff(json1, json2, ignore_order=True)
print("OK!" if not diff else diff)