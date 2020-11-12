#ifndef __CODE_GENERATION_H
#define __CODE_GENERATION_H 

inline static void generate_main_python(struct parser *p) {


    FILE *c_file = p->c_file;

	int num_odes = shlen(p->ode_identifiers);

	fprintf(c_file, "from scipy.integrate import odeint\n"),
	fprintf(c_file, "from math import floor\n");
	fprintf(c_file, "from math import tanh\n");
	fprintf(c_file, "from math import log\n");
	fprintf(c_file, "from math import exp\n");
	fprintf(c_file, "from sys import argv\n");

	fprintf(c_file, "def fabs(a):\n");
   	fprintf(c_file, "    return abs(a)\n\n");


	fprintf(c_file, "def set_initial_conditions(x0):\n");


	for(int i = 0; i < num_odes; i++) {
		double value = shget(p->ode_intial_values,  p->ode_identifiers[i].key);
		fprintf(c_file, "    x0.append(%lf)\n", value);
	}

    fprintf(c_file, "\ndef model(sv, time):\n");

	for(int i = 0; i < num_odes; i++) {
		fprintf(c_file, "    %s = sv[%d]\n", p->ode_identifiers[i].key, i);
	}

	fprintf(c_file, "    rDY = [0 for i in range(%d)]\n", num_odes);
	fprintf(c_file, "%s", p->ode_code);
	fprintf(c_file, "    return rDY\n\n");

    fprintf(c_file, "if __name__ == \"__main__\":\n");

    fprintf(c_file, "    x0 = []\n");
    fprintf(c_file, "    ts = range(int(argv[1]))\n");

    fprintf(c_file, "    set_initial_conditions(x0)\n");

    fprintf(c_file, "    result = odeint(model, x0, ts)\n");

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
