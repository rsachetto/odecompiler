#ifndef __CODE_GENERATION_H
#define __CODE_GENERATION_H 

inline static void generate_main_python(FILE *c_file, int num_odes) {
    fprintf(c_file, "if __name__ == \"__main__\":\n");

    fprintf(c_file, "    x0 = []\n");
    fprintf(c_file, "    ts = range(int(argv[1]))\n");

    fprintf(c_file, "    set_initial_conditions(x0)\n");

    fprintf(c_file, "    result = odeint(ode, x0, ts)\n");

    fprintf(c_file, "    out = open(\"out.txt\", \"w\")\n");

    fprintf(c_file, "    for i in range(len(ts)):\n");
    fprintf(c_file, "        out.write(str(ts[i]) + \", \")\n");
    fprintf(c_file, "        for j in range(%d):\n", num_odes);
    fprintf(c_file, "            if j < 1:\n");
    fprintf(c_file, "                out.write(str(result[i, j]) + \", \")\n");
    fprintf(c_file, "            else:\n");
    fprintf(c_file, "                out.write(str(result[i, j]))\n");

    fprintf(c_file, "        out.write(\"\\n\")\n");

    fprintf(c_file, "    out.close()\n");

    fprintf(c_file, "    try:\n");
    fprintf(c_file, "        import pylab\n");
    fprintf(c_file, "        pylab.figure(1)\n");
    fprintf(c_file, "        pylab.plot(ts, result[:,0])\n");
    fprintf(c_file, "        pylab.show()\n");
    fprintf(c_file, "    except ImportError:\n");
    fprintf(c_file, "        pass\n");
}

#endif /* __CODE_GENERATION_H */
