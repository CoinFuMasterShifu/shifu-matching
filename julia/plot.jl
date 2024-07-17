using Plots

qp =10
bp =10
qi = 5
bi = 3
qd(bd) = - qp*bd/(bp+bd)
q1(bd) = (qp*bp)/(bp+bd)^2
q2(bd) = qi/(bi-bd)

p1(bd) = (qp*bp)*(bi-bd)
p2(bd) = qi*(bp+bd)^2

x0 = -20
x1 = 20
plot(p1,xlim = [x0,x1])
plot!(p2,xlim = [x0,x1])

plot(q1,xlim = [x0,x1], ylim = [0,10])
plot!(q2,xlim = [x0,x1])

