from pathlib import Path
root = Path(__file__).resolve().parents[1]
html = (root / 'web/dashboard.html').read_text(encoding='utf-8')
assert ')KLDASH"' not in html
(root / 'src/dashboard.hpp').write_text('#pragma once\ninline constexpr const char* dashboardTemplate=R"KLDASH('+html+')KLDASH";\n',encoding='utf-8')
parts = []
for name in ['third_party/json.hpp', 'src/core.hpp', 'src/client.hpp', 'src/dashboard.hpp', 'src/anomaly.hpp', 'src/main.cpp']:
    text = (root / name).read_text(encoding='utf-8')
    if not name.startswith('third_party/'):
        text = '\n'.join(line for line in text.splitlines() if line != '#pragma once' and line not in ['#include "../third_party/json.hpp"', '#include "core.hpp"', '#include "client.hpp"', '#include "dashboard.hpp"', '#include "anomaly.hpp"'])
    parts.append('\n'+text+'\n')
(root / 'KalshiLens.cpp').write_text(''.join(parts),encoding='utf-8')
print('Regenerated dashboard.hpp and KalshiLens.cpp')
