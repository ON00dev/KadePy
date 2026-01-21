import os

files = [f for f in os.listdir('tests') if f.endswith('.py')]
for f in files:
    path = os.path.join('tests', f)
    try:
        # Try reading as UTF-16 (PowerShell default)
        with open(path, 'r', encoding='utf-16') as fp:
            content = fp.read()
    except UnicodeError:
        try:
            # Maybe it was UTF-8?
            with open(path, 'r', encoding='utf-8') as fp:
                content = fp.read()
        except:
            print(f"Skipping {f}, unknown encoding")
            continue
            
    # Write back as UTF-8
    with open(path, 'w', encoding='utf-8') as fp:
        fp.write(content)
    print(f"Fixed {f}")
