
\echo -- q1
SELECT * FROM pgr_turnPenaltyGraph(
    'SELECT id, source, target, cost, reverse_cost FROM edge_table'
);

\echo -- q2
SELECT * FROM pgr_turnPenaltyGraph(
    'SELECT id, source, target, cost, reverse_cost FROM edge_table',
    FALSE
);
\echo -- q3
