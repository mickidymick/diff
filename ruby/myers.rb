#!/usr/bin/env ruby

Box = Struct.new(:left, :top, :right, :bottom) do
    def width
        right - left
    end

    def height
        bottom - top
    end

    def size
        width + height
    end

    def delta
        width - height
    end
end

class MyersLinear
    def self.diff(a, b)
        new(a, b).diff
    end

    def initialize(a, b)
        @a, @b = a, b
    end

    def forwards(box, vf, vb, d)
        (-d .. d).step(2).reverse_each do |k|
            c = k - box.delta

            if k == -d or (k != d and vf[k - 1] < vf[k + 1])
                px = x = vf[k + 1]
            else
                px = vf[k - 1]
                x  = px + 1
            end

            y  = box.top + (x - box.left) - k
            py = (d == 0 || x != px) ? y : y - 1

            while x < box.right and y < box.bottom and @a[x] == @b[y]
                x, y = x + 1, y + 1
            end

            vf[k] = x

            if box.delta.odd? and c.between?(-(d - 1), d - 1) and y >= vb[c]
                yield [[px, py], [x, y]]
            end
        end
    end

    def backward(box, vf, vb, d)
        (-d .. d).step(2).reverse_each do |c|
            k = c + box.delta

            if c == -d or (c != d and vb[c - 1] > vb[c + 1])
                py = y = vb[c + 1]
            else
                py = vb[c - 1]
                y  = py - 1
            end

            x  = box.left + (y - box.top) + k
            px = (d == 0 || y != py) ? x : x + 1

            while x > box.left and y > box.top and @a[x - 1] == @b[y - 1]
                x, y = x - 1, y - 1
            end

            vb[c] = y

            if box.delta.even? and k.between?(-d, d) and x <= vf[k]
                yield [[x, y], [px, py]]
            end
        end
    end

    def midpoint(box)
        return nil if box.size == 0

        max = (box.size / 2.0).ceil

        vf    = Array.new(2 * max + 1)
        vf[1] = box.left
        vb    = Array.new(2 * max + 1)
        vb[1] = box.bottom

        (0 .. max).step do |d|
            forwards(box, vf, vb, d) { |snake| return snake }
            backward(box, vf, vb, d) { |snake| return snake }
        end
    end

    def find_path(left, top, right, bottom)
        box   = Box.new(left, top, right, bottom)
        snake = midpoint(box)

        return nil unless snake

        start, finish = snake

        head = find_path(box.left, box.top, start[0], start[1])
        tail = find_path(finish[0], finish[1], box.right, box.bottom)

        (head || [start]) + (tail || [finish])
    end

    def diff
        print find_path(0, 0, 14, 14)
    end
end

a = [
"void Chunk_copy(Chunk *src, size_t src_start, Chunk *dst, size_t dst_start, size_t n)",
"{",
"    if (!Chunk_bounds_check(src, src_start, n)) return;",
"    if (!Chunk_bounds_check(dst, dst_start, n)) return;",
"",
"    memcpy(dst->data + dst_start, src->data + src_start, n);",
"}",
"",
"int Chunk_bounds_check(Chunk *chunk, size_t start, size_t n)",
"{",
"    if (chunk == NULL) return 0;",
"",
"    return start <= chunk->length && n <= chunk->length - start;",
"}"
]

b = [
"    int Chunk_bounds_check(Chunk *chunk, size_t start, size_t n)",
"{",
"    if (chunk == NULL) return 0;",
"",
"    return start <= chunk->length && n <= chunk->length - start;",
"}",
"",
"void Chunk_copy(Chunk *src, size_t src_start, Chunk *dst, size_t dst_start, size_t n)",
"{",
"    if (!Chunk_bounds_check(src, src_start, n)) return;",
"    if (!Chunk_bounds_check(dst, dst_start, n)) return;",
"",
"    memcpy(dst->data + dst_start, src->data + src_start, n);",
"}"
]

m = MyersLinear. new(a, b)
m.diff
