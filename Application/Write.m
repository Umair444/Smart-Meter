for i = 1:10
    x = 50 + 500*rand;
    y = 50 + 500*rand;
    z = thingSpeakWrite(902207, [x,y], 'WriteKey', '6N9WQF4HT0U2ZHX8');
    disp(z.Field1);
    disp(z.Field2);
    pause(15);
end