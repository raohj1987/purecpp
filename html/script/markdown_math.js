(function (window) {
    'use strict';

    function escapeHtml(value) {
        return String(value)
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;')
            .replace(/"/g, '&quot;')
            .replace(/'/g, '&#39;');
    }

    function escapeRegExp(value) {
        return value.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
    }

    function normalizeMarkdownSource(content) {
        if (content === null || content === undefined) return '';
        return String(content)
            .replace(/\\n/g, '\n')
            .replace(/\\t/g, '\t')
            .replace(/\r\n?/g, '\n');
    }

    function isEscaped(source, index) {
        let slashCount = 0;
        for (let i = index - 1; i >= 0 && source[i] === '\\'; i--) {
            slashCount++;
        }
        return slashCount % 2 === 1;
    }

    function isLineStart(source, index) {
        return index === 0 || source[index - 1] === '\n';
    }

    function readFencedCode(source, start) {
        const open = /^( {0,3})(`{3,}|~{3,})[^\n]*(?:\n|$)/.exec(source.slice(start));
        if (!open) return null;

        const fence = open[2];
        const fenceChar = fence[0];
        const closingFence = new RegExp('^ {0,3}' + escapeRegExp(fence) + escapeRegExp(fenceChar) + '*\\s*$');
        let pos = start + open[0].length;

        while (pos < source.length) {
            const lineEnd = source.indexOf('\n', pos);
            const nextPos = lineEnd === -1 ? source.length : lineEnd + 1;
            const line = source.slice(pos, lineEnd === -1 ? source.length : lineEnd);

            if (closingFence.test(line)) {
                return source.slice(start, nextPos);
            }

            pos = nextPos;
        }

        return source.slice(start);
    }

    function readInlineCode(source, start) {
        const open = /^`+/.exec(source.slice(start));
        if (!open) return null;

        const ticks = open[0];
        const close = source.indexOf(ticks, start + ticks.length);
        if (close === -1) return null;
        return source.slice(start, close + ticks.length);
    }

    function findClosingDelimiter(source, start, delimiter, allowNewline) {
        let pos = start;
        while (pos < source.length) {
            const close = source.indexOf(delimiter, pos);
            if (close === -1) return -1;
            if (!isEscaped(source, close)) {
                if (!allowNewline && source.slice(start, close).includes('\n')) return -1;
                return close;
            }
            pos = close + delimiter.length;
        }
        return -1;
    }

    function canOpenInlineDollar(source, index) {
        const next = source[index + 1];
        if (!next || next === '$' || /\s/.test(next)) return false;
        return !isEscaped(source, index);
    }

    function canCloseInlineDollar(source, index) {
        const prev = source[index - 1];
        const next = source[index + 1];
        if (!prev || /\s/.test(prev)) return false;
        if (next && /\d/.test(next)) return false;
        return true;
    }

    function findInlineDollarClose(source, start) {
        let pos = start;
        while (pos < source.length) {
            const close = source.indexOf('$', pos);
            if (close === -1) return -1;
            if (!isEscaped(source, close) && source[close + 1] !== '$' && canCloseInlineDollar(source, close)) {
                return close;
            }
            pos = close + 1;
        }
        return -1;
    }

    function renderMath(tex, displayMode, raw) {
        const math = tex.trim();
        if (!math) return escapeHtml(raw);

        if (!window.temml || typeof window.temml.renderToString !== 'function') {
            return escapeHtml(raw);
        }

        try {
            const rendered = window.temml.renderToString(math, {
                displayMode: displayMode,
                throwOnError: false,
                trust: false
            });
            const className = displayMode ? 'math-display' : 'math-inline';
            return '<span class="' + className + '">' + rendered + '</span>';
        } catch (error) {
            console.warn('Math render failed:', error);
            return '<code class="math-error">' + escapeHtml(raw) + '</code>';
        }
    }

    function protectMath(source) {
        const items = [];
        let output = '';
        let i = 0;

        function addMath(tex, displayMode, raw) {
            const token = '@@PURECPPMATH' + items.length + '@@';
            items.push({
                token: token,
                html: renderMath(tex, displayMode, raw)
            });
            return token;
        }

        while (i < source.length) {
            if (isLineStart(source, i)) {
                const fenced = readFencedCode(source, i);
                if (fenced) {
                    output += fenced;
                    i += fenced.length;
                    continue;
                }
            }

            if (source[i] === '`') {
                const inlineCode = readInlineCode(source, i);
                if (inlineCode) {
                    output += inlineCode;
                    i += inlineCode.length;
                    continue;
                }
            }

            if (source.startsWith('$$', i) && !isEscaped(source, i)) {
                const close = findClosingDelimiter(source, i + 2, '$$', true);
                if (close !== -1) {
                    const raw = source.slice(i, close + 2);
                    output += addMath(source.slice(i + 2, close), true, raw);
                    i = close + 2;
                    continue;
                }
            }

            if (source.startsWith('\\[', i) && !isEscaped(source, i)) {
                const close = findClosingDelimiter(source, i + 2, '\\]', true);
                if (close !== -1) {
                    const raw = source.slice(i, close + 2);
                    output += addMath(source.slice(i + 2, close), true, raw);
                    i = close + 2;
                    continue;
                }
            }

            if (source.startsWith('\\(', i) && !isEscaped(source, i)) {
                const close = findClosingDelimiter(source, i + 2, '\\)', false);
                if (close !== -1) {
                    const raw = source.slice(i, close + 2);
                    output += addMath(source.slice(i + 2, close), false, raw);
                    i = close + 2;
                    continue;
                }
            }

            if (source[i] === '$' && canOpenInlineDollar(source, i)) {
                const close = findInlineDollarClose(source, i + 1);
                if (close !== -1) {
                    const raw = source.slice(i, close + 1);
                    output += addMath(source.slice(i + 1, close), false, raw);
                    i = close + 1;
                    continue;
                }
            }

            output += source[i];
            i++;
        }

        return {
            source: output,
            items: items
        };
    }

    function restoreMath(html, items) {
        return items.reduce(function (result, item) {
            return result.split(item.token).join(item.html);
        }, html);
    }

    function renderMarkdown(content, markedOptions) {
        const source = normalizeMarkdownSource(content);
        const protectedMath = protectMath(source);
        const html = window.marked && typeof window.marked.parse === 'function'
            ? window.marked.parse(protectedMath.source, markedOptions)
            : escapeHtml(protectedMath.source);

        return restoreMath(html, protectedMath.items);
    }

    window.PureCppMarkdown = {
        normalizeMarkdownSource: normalizeMarkdownSource,
        renderMarkdown: renderMarkdown
    };
    window.renderMarkdown = renderMarkdown;
})(window);
