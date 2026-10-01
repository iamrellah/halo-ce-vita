"""Actual C translation units, excluding documented linker bookkeeping records."""
PSEUDO_UNITS = {
    'source/linker_common.c': 'Pooled COMMON metadata, not a C source unit; see docs/linker_common_comparison_base_20260922.md'
}

def engine_units(project):
    return [item['name'] for item in project['objects']
            if item['name'].endswith('.c') and item['status'] != 'Missing'
            and item['name'] not in PSEUDO_UNITS]
