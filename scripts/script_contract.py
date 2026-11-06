"""Compare how the game's scripts call engine functions with what the server gives them.

Takes a luac listing of every script, the check's script-api.txt and the server source, and
writes every mismatch: functions scripts call that do not exist, and adapter functions that take
fewer arguments or give back fewer values than scripts use.

C++ bindings are not compared on arguments, because their types are not visible here.
"""
import collections
import pathlib
import re
import sys

ENGINE_TABLE = re.compile(r'^n[A-Z][A-Za-z]*$')
BINDINGS = {'object': 'nGameObject', 'ability': 'nAbility', 'gameAbility': 'nAbility', 'locomotion': 'nLocomotion'}
VARIABLE = 0


def read_calls(listing):
    """Count calls as (name, arguments, results). Either count is None when it is open ended."""
    calls = collections.Counter()
    pending = {}
    for line in listing.read_text(errors='replace').splitlines():
        if line.startswith(('function <', 'main <', '### ')):
            pending = {}
            continue

        parts = line.split()
        if len(parts) < 5 or parts[1] != '[-]':
            continue

        op, register = parts[2], parts[3]
        if op == 'GETGLOBAL' and ';' in parts:
            name = parts[-1]
            pending[register] = name if ENGINE_TABLE.match(name) else None
        elif op == 'GETTABLE' and parts[4] == register and pending.get(register) and '"' in line:
            field = line.rsplit('"', 2)[1]
            pending[register] = pending[register] + '.' + field
        elif op == 'CALL':
            name = pending.pop(register, None)
            if name and '.' in name:
                arguments, results = int(parts[4]), int(parts[5])
                calls[(name, None if arguments == VARIABLE else arguments - 1,
                       None if results == VARIABLE else results - 1)] += 1
    return calls


def split_top_level(text):
    pieces, depth, current = [], 0, ''
    for character in text:
        depth += character in '([{'
        depth -= character in ')]}'
        if character == ',' and depth == 0:
            pieces.append(current)
            current = ''
        else:
            current += character
    return pieces + [current]


def read_adapter(source):
    """Find each adapter function's parameter count and the most values it returns."""
    text = source.read_text(errors='replace')
    functions = {}
    for match in re.finditer(r'^(\t+)function (n[A-Z]\w*)\.(\w+)\(([^)]*)\)\n(.*?)^\1end\b', text, re.M | re.S):
        indent, table, name, parameters, body = match.groups()
        names = [p.strip() for p in parameters.split(',') if p.strip()]
        returns = 0
        for statement in re.findall(r'\breturn\b(.*)', body):
            values = split_top_level(statement.strip())
            if statement.strip() and len(values) == 1 and values[0].rstrip().endswith(')'):
                returns = None  # hands back whatever another call gives
                break
            returns = max(returns, len(values) if statement.strip() else 0)
        functions[table + '.' + name] = ('...' in names, len(names), returns)
    return functions


def read_known_names(api_list, bindings):
    known = {line.strip() for line in api_list.read_text(errors='replace').splitlines()}
    text = bindings.read_text(errors='replace')
    for variable, name in re.findall(r'\b(\w+)\["(\w+)"\]\s*=', text):
        if variable in BINDINGS:
            known.add(BINDINGS[variable] + '.' + name)
    return known


def main():
    if len(sys.argv) != 5:
        sys.exit('usage: script_contract.py <luac listing> <script-api.txt> <source/game folder> <report>')

    listing, api_list, source, report = (pathlib.Path(argument) for argument in sys.argv[1:])
    calls = read_calls(listing)
    adapter = read_adapter(source / 'scriptapi.cpp')
    known = read_known_names(api_list, source / 'luafunctions.cpp') | set(adapter)

    uses = collections.Counter()
    most_arguments = collections.Counter()
    most_results = collections.Counter()
    for (name, arguments, results), count in calls.items():
        uses[name] += count
        most_arguments[name] = max(most_arguments[name], arguments or 0)
        most_results[name] = max(most_results[name], results or 0)

    missing = sorted((n for n in uses if n not in known), key=lambda n: (-uses[n], n))

    lines = ['MISSING %s used %d' % (name, uses[name]) for name in missing]
    mismatches = 0
    for name in sorted(adapter):
        if name not in uses:
            continue
        variadic, parameters, returns = adapter[name]
        if not variadic and most_arguments[name] > parameters:
            mismatches += 1
            lines.append('ARGUMENTS %s takes %d, scripts pass up to %d' % (name, parameters, most_arguments[name]))
        if returns is not None and most_results[name] > returns:
            mismatches += 1
            lines.append('RESULTS %s gives %d, scripts read up to %d' % (name, returns, most_results[name]))

    report.write_text('\n'.join(lines) + '\n')
    print('Contract: %d engine functions used, %d missing, %d adapter mismatches' % (len(uses), len(missing), mismatches))


if __name__ == '__main__':
    main()
