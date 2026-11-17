from platformio.test.runners.base import TestRunnerBase
from platformio.test.result import TestCase, TestStatus


class CustomTestRunner(TestRunnerBase):
    def on_testing_line_output(self, line):
        super().on_testing_line_output(line)
        if line.strip() == "All GCS bench checks passed":
            self.test_suite.add_case(TestCase(name="GCS bench checks", status=TestStatus.PASSED))
