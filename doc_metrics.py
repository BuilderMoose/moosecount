#!/usr/bin/env python3
import argparse
import re
from pathlib import Path
from collections import defaultdict

def parse_markdown(content):
    """Extracts structural metrics from Markdown/Text files."""
    lines = content.splitlines()
    words = len(content.split())
    
    headers = len(re.findall(r'^#{1,6}\s', content, re.MULTILINE))
    todos_open = len(re.findall(r'^\s*-\s*\[ \]', content, re.MULTILINE))
    todos_done = len(re.findall(r'^\s*-\s*\[[xX]\]', content, re.MULTILINE))
    
    return {
        'Lines': len(lines),
        'Words': words,
        'Headers': headers,
        'Open Tasks': todos_open,
        'Completed Tasks': todos_done
    }

def parse_plantuml(content):
    """Extracts architectural metrics from PlantUML files."""
    lines = content.splitlines()
    
    # Matches common UML entities (class, component, actor, usecase, state, node)
    entities = len(re.findall(r'^\s*(?:class|component|actor|usecase|state|node|interface)\s+', content, re.IGNORECASE | re.MULTILINE))
    
    # Matches UML relationships (->, -->, ..>, <|--, etc.)
    relationships = len(re.findall(r'[-.]{1,2}\>', content)) + len(re.findall(r'\<[-.]{1,2}', content))
    
    return {
        'Lines': len(lines),
        'UML Entities': entities,
        'Relationships': relationships
    }

def main():
    parser = argparse.ArgumentParser(description="Extract metrics from design and journal files.")
    parser.add_argument('paths', nargs='*', default=['.'], help="Paths to search")
    parser.add_argument('--exclude', action='append', default=[], help="Folder names to exclude")
    args = parser.parse_args()

    # Default exclusions matching your C++ workflow
    exclusions = set(['build', 'bin', '.git', '.vscode'] + args.exclude)
    
    md_totals = defaultdict(int)
    puml_totals = defaultdict(int)
    file_counts = {'Markdown/Text': 0, 'PlantUML': 0}

    print(f"{'File':<50} | {'Metrics'}")
    print("-" * 80)

    for base_path in args.paths:
        path_obj = Path(base_path)
        if not path_obj.exists():
            print(f"Warning: Path not found {base_path}")
            continue

        # Iterate through files, respecting exclusions
        for file_path in path_obj.rglob('*'):
            if not file_path.is_file():
                continue
                
            # Check if any parent directory is in the exclusion list
            if any(part in exclusions for part in file_path.parts):
                continue

            ext = file_path.suffix.lower()
            
            try:
                content = file_path.read_text(encoding='utf-8', errors='ignore')
            except Exception as e:
                continue

            if ext in ['.md', '.txt']:
                metrics = parse_markdown(content)
                file_counts['Markdown/Text'] += 1
                for k, v in metrics.items():
                    md_totals[k] += v
                
                metric_str = f"Words: {metrics['Words']} | Tasks: {metrics['Completed Tasks']}/{metrics['Open Tasks'] + metrics['Completed Tasks']}"
                print(f"{str(file_path):<50} | {metric_str}")

            elif ext in ['.puml', '.pu', '.wsd']:
                metrics = parse_plantuml(content)
                file_counts['PlantUML'] += 1
                for k, v in metrics.items():
                    puml_totals[k] += v
                
                metric_str = f"Entities: {metrics['UML Entities']} | Relationships: {metrics['Relationships']}"
                print(f"{str(file_path):<50} | {metric_str}")

    print("\n" + "=" * 80)
    print("TOTALS")
    print("=" * 80)
    
    print(f"Markdown & Text Files: {file_counts['Markdown/Text']}")
    for k, v in md_totals.items():
        print(f"  - {k:<15}: {v}")
        
    print(f"\nPlantUML Files: {file_counts['PlantUML']}")
    for k, v in puml_totals.items():
        print(f"  - {k:<15}: {v}")

if __name__ == '__main__':
    main()
