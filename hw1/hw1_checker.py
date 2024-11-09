import json
import sys
from collections import Counter

def load_json(file_path):
    with open(file_path, 'r') as file:
        return json.load(file)

def compare_dependences(correct, user):
    correct_counter = Counter(tuple(dep.items()) for dep in correct)
    user_counter = Counter(tuple(dep.items()) for dep in user)

    missing = list((correct_counter - user_counter).elements())
    extra = list((user_counter - correct_counter).elements())

    return missing, extra

def report_results(missing, extra):
    if not missing and not extra:
        print("Your answer is correct!")
    else:
        if missing:
            print(f"Missing parts ({len(missing)}):")
            for m in missing:
                print(dict(m))
        if extra:
            print(f"Extra parts ({len(extra)}):")
            for e in extra:
                print(dict(e))

def main(correct_file, user_file):
    # Load correct answer and user answer
    correct_data = load_json(correct_file)
    user_data = load_json(user_file)

    total_missing = []
    total_extra = []

    # Compare each dependency section
    flow_missing, flow_extra = compare_dependences(correct_data.get("FlowDependence", []), user_data.get("FlowDependence", []))
    total_missing.extend(flow_missing)
    total_extra.extend(flow_extra)

    anti_missing, anti_extra = compare_dependences(correct_data.get("AntiDependence", []), user_data.get("AntiDependence", []))
    total_missing.extend(anti_missing)
    total_extra.extend(anti_extra)

    output_missing, output_extra = compare_dependences(correct_data.get("OutputDependence", []), user_data.get("OutputDependence", []))
    total_missing.extend(output_missing)
    total_extra.extend(output_extra)

    # Report results
    print("====Flow Dependences====")
    report_results(flow_missing, flow_extra)

    print("\n====Antidependences====")
    report_results(anti_missing, anti_extra)

    print("\n====Output Dependence====")
    report_results(output_missing, output_extra)

    # Report total missing and extra across all dependencies
    print("\n====Overall Report====")
    if not total_missing and not total_extra:
        print("Your answer is correct!")
    else:
        print(f"Total missing parts: {len(total_missing)}")
        print(f"Total extra parts: {len(total_extra)}")
        print(f"Total wrong parts: {len(total_missing) + len(total_extra)}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python3 script.py correct_answer.json your_output.json")
        sys.exit(1)

    correct_file = sys.argv[1]
    user_file = sys.argv[2]
    main(correct_file, user_file)