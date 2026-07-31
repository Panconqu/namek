-- NAMEK MOD: api_benchmarker_plus v3.0.0
-- Benchmarks sintéticos del motor: mide tiempo de UUID, hashing y operaciones
-- NamekDB usando el reloj restringido del sandbox (os.time/os.clock).
namek.register_command("benchmark_mod", function(cmd, args)
    local n = tonumber(args[1]) or 1000
    namek.print("Benchmark Namek engine | " .. n .. " iteraciones")

    local t0 = os.clock() or 0
    local acc = 0
    for i = 1, n do
        local u = namek.uuid()
        acc = acc + string.len(u)
    end
    local t1 = os.clock() or 0
    namek.print("  uuid()  x" .. n .. ": " .. string.format("%.4f", t1 - t0) .. "s")

    local t2 = os.clock() or 0
    for i = 1, n do
        namek.sha256("bench-" .. tostring(i))
    end
    local t3 = os.clock() or 0
    namek.print("  sha256  x" .. n .. ": " .. string.format("%.4f", t3 - t2) .. "s")

    local t4 = os.clock() or 0
    for i = 1, n do
        namek.db_set("bench.key" .. tostring(i % 50), "v")
    end
    local t5 = os.clock() or 0
    namek.print("  db_set  x" .. n .. ": " .. string.format("%.4f", t5 - t4) .. "s")
    namek.print("Benchmark completo | checksum=" .. namek.md5("bench-" .. tostring(n)))
end)

namek.print("api_benchmarker_plus v3.0.0 listo | comando: tb benchmark_mod <n>")
